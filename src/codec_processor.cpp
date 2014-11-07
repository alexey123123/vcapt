#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>

#include "codec_processor.h"
#include "camera_container.h"
#include "filter.h"


#define FPS_INTEGRATION_MS 5000
#define INTERFRAME_INTERVAL_MS 30
#define FRAME_GENERATOR_CLEANUP_TIMEOUT 5



codec_processor::codec_processor(camera_container* _ccont, codec_ptr _cod,stop_handler _sh):
	_camera_container(_ccont),_codec(_cod),_stop_handler(_sh),
	work_timer(internal_ioservice),
	check_clients_timer(internal_ioservice),
	fps_meter_timer(internal_ioservice),
	sum_value(0),
	max_value(0),
	min_value(0xFFFFFFFF),
	encoded_frame_counter(0),
	empty_get_frame_calls(0),
	last_packet_pts(0),
	encoder_fps(0),
	exceptions_count(0),
	streams_count(0),
	last_error_fs(camera_container::fs_Ok),
	frame_generator_cleanup_timer(internal_ioservice),
	
	summary_gf2_time_ms(0),
	summary_gf2_count(0),
	gf2_average(0)
	{
	
		work_timer.expires_from_now(boost::chrono::milliseconds(50));
		work_timer.async_wait(boost::bind(&codec_processor::do_processor_work,this,boost::asio::placeholders::error));

		fps_meter_timer.expires_from_now(boost::chrono::milliseconds(FPS_INTEGRATION_MS));
		fps_meter_timer.async_wait(boost::bind(&codec_processor::do_fps_meter,this,boost::asio::placeholders::error));

		last_frame_tp = boost::chrono::steady_clock::now();

		frame_generator_cleanup_timer.expires_from_now(boost::chrono::seconds(5));
		frame_generator_cleanup_timer.async_wait(boost::bind(&codec_processor::i_thread_frame_generator_cleanup,
			this,boost::asio::placeholders::error));


	internal_thread = boost::thread(boost::bind(&codec_processor::internal_thread_proc,this));

	printf("----codec_processor new!\n");
}


codec_processor::~codec_processor(){
	internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>();

	BOOST_FOREACH(stream_ptr sp,streams)
		sp->signal_to_stop();

	work_timer.cancel();
	check_clients_timer.cancel();
	fps_meter_timer.cancel();
	frame_generator_cleanup_timer.cancel();
	
	
	internal_thread.join();
	printf("----codec_processor free!\n");

}

#define MAX_STREAM_EXCEPTIONS_COUNT 10

void codec_processor::do_processor_work(boost::system::error_code ec){
		if (ec)
			return ;

		if (!internal_ioservice_work_ptr)
			return ;
		if (streams_count > 0)
		try{



			using namespace boost::chrono;
			high_resolution_clock::time_point start = high_resolution_clock::now();

			//capture frame
			frame_ptr fptr;
			camera_container::frame_state fs = _camera_container->get_frame2(_codec->get_format().fsize, _codec->get_format().input_pixfmt, last_frame_tp,fptr);
			if (!fptr)
				empty_get_frame_calls++;
			if (fs != camera_container::fs_Ok){
				
				//generate alarm picture
				if (fs != (camera_container::frame_state)last_error_fs){
					last_error_title = "";
					switch(fs){
						case camera_container::fs_FramesizeOrFormatDiffers:
							last_error_title = "Framesize changed.\nNeed restart stream";
							break;
						case camera_container::fs_CapturerNotInitialized:
							last_error_title = "Camera not initialized";
							break;
					}
					last_error_fs = fs;					
				}
				fptr = _frame_generator.get(_codec->get_format().fsize,last_error_title,300,last_frame_tp);
				if (fptr)
					last_frame_tp = fptr->tp;				
			}
			high_resolution_clock::time_point stop = high_resolution_clock::now();
			uint64_t dur1_ms = duration_cast<milliseconds> (stop - start).count();
			summary_gf2_time_ms += dur1_ms; 

			packet_ptr pptr;
			if (fptr)
				pptr = _codec->process_frame(fptr);

			if (pptr){
				pptr->frame_tp = fptr->tp;
				last_frame_tp = fptr->tp;

				unsigned int dur2_ms = pptr->p.pts - last_packet_pts;
				last_packet_pts = pptr->p.pts;
				
				if (fptr->capturer_state != capturer::st_Ready)
					pptr->packet_flags |= PACKET_FLAG_CAPTURER_ERROR_BIT;
				if (fs != camera_container::fs_Ok)
					pptr->packet_flags |= PACKET_FLAG_STREAM_ERROR_BIT;

				stop = high_resolution_clock::now();
				dur1_ms = duration_cast<milliseconds> (stop - fptr->tp).count();


				BOOST_FOREACH(stream_ptr sptr, streams){
					try{
						sptr->process_packet(pptr);
					}
					catch(...){
						std::cout<<"stream fail"<<std::endl;
						internal_ioservice.post(boost::bind(&codec_processor::i_thread_delete_stream,this,sptr.get()));
					}

				}


				//some statistics
				//unsigned int dur2_ms = duration_cast<milliseconds> (stop - fptr->tp).count();

				sum_value += dur1_ms;
				if (dur1_ms > max_value)
					max_value = dur1_ms;
				if (dur1_ms < min_value)
					min_value = dur1_ms;

				encoded_frame_counter++;
			}




			exceptions_count = 0;
		}
		catch(std::runtime_error& ex){
			exceptions_count++;
			std::cout<<"codec_processor exception:"<<ex.what()<<std::endl;

			if (exceptions_count > MAX_STREAM_EXCEPTIONS_COUNT){
				using namespace Utility;
				Journal::Instance()->Write(ALERT,DST_SYSLOG|DST_STDERR,"codec thread exceeded exception limit");
				//finalize stream
				call_stop_handler("many runtime errors:"+std::string(ex.what()));

				return ;
			}
		}
		catch(...){
			//fatal exception
			using namespace Utility;
			Journal::Instance()->Write(ALERT,DST_SYSLOG|DST_STDERR,"codec thread stopped by unknown exception");

			//finalize stream
			call_stop_handler("unknown exception");


			return ;
		}

		work_timer.expires_from_now(boost::chrono::milliseconds(INTERFRAME_INTERVAL_MS));
		work_timer.async_wait(boost::bind(&codec_processor::do_processor_work,this,boost::asio::placeholders::error));
	}



bool codec_processor::try_to_add_network_client(client_parameters e_params, tcp_client_ptr tcptr){
	if (e_params.codec_id != _codec->get_format().codec_id)
		return false;

	if (e_params.f_size)
		if (*e_params.f_size != _codec->get_format().fsize)
			return false;

	if (e_params.bitrate)
		if (*e_params.bitrate != _codec->get_format().bitrate)
			return false;

	//create network stream
	stream_ptr sptr = av_container_stream::create_network_stream(e_params,_codec->get_avcodec_context(),
		boost::bind(&codec_processor::delete_stream,this,_1),
		tcptr);
	sptr->start();

	add_stream(sptr);



	return true;
}

void codec_processor::add_stream(stream_ptr sptr){
	internal_ioservice.post(boost::bind(&codec_processor::i_thread_add_stream,this,sptr));
}

void codec_processor::i_thread_add_stream(stream_ptr sptr){
	streams.push_back(sptr);

	//have header packets ?
	std::deque<packet_ptr> start_packets;
	_codec->get_header_packets(start_packets);
	BOOST_FOREACH(packet_ptr p,start_packets){
		p->frame_tp = boost::chrono::steady_clock::now();
		sptr->process_packet(p);
	}



	streams_count.exchange(streams.size(),boost::memory_order_release);
}


void codec_processor::internal_thread_proc(){
	try{
		internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>(
			new boost::asio::io_service::work(internal_ioservice));
		internal_ioservice.run();
	}
	catch(...){
		using namespace Utility;
		Journal::Instance()->Write(ALERT,DST_STDERR|DST_SYSLOG,"unhandled exception in codec_processor::internal_thread");
	}
}

void codec_processor::delete_stream(stream* s){
	internal_ioservice.post(boost::bind(&codec_processor::i_thread_delete_stream,this,s));
}
void codec_processor::i_thread_delete_stream(stream* s){
	std::deque<stream_ptr> remained_streams;
	stream_ptr sptr;
	BOOST_FOREACH(sptr,streams)
		if (sptr.get() != s)
			remained_streams.push_back(sptr);
	sptr = stream_ptr();
	streams = remained_streams;
	streams_count.exchange(streams.size(),boost::memory_order_release);

	if (streams.size()==0){
		check_clients_timer.expires_from_now(boost::chrono::milliseconds(2000));
		check_clients_timer.async_wait(boost::bind(&codec_processor::check_clients_and_finalize_processor,this,boost::asio::placeholders::error));

	}

}


void codec_processor::check_clients_and_finalize_processor(boost::system::error_code ec){
	if (ec)
		return ;
	if (!internal_ioservice_work_ptr)
		return ;


	if (streams_count == 0){
		call_stop_handler("no active clients");
		_stop_handler(this);
	}
		
}

void codec_processor::do_fps_meter(boost::system::error_code ec){
	if (ec)
		return ;

	if (!internal_ioservice_work_ptr)
		return ;





	uint64_t avg_value = -1;
	int new_fps = -1;
	unsigned int af2_a = 0;
	if (encoded_frame_counter > 0){
		avg_value = sum_value / encoded_frame_counter;
		encoder_fps = encoded_frame_counter / (FPS_INTEGRATION_MS / 1000);
		gf2_average = af2_a;
		std::cout<<	"codec processor: fps: "<<encoder_fps<<
			"(count:"<<encoded_frame_counter<<","<<
			",empty:"<<empty_get_frame_calls<<","<<

			"avg value:"<<avg_value<<
			",min:"<<min_value<<
			",max:"<<max_value<<
			"), gf2_time:"<<gf2_average<<std::endl;

		encoded_frame_counter = 0;
		sum_value = 0;
		summary_gf2_time_ms = 0;
		empty_get_frame_calls = 0;

		max_value = 0;
		min_value = 0xFFFFFFFF;
	}
		




	fps_meter_timer.expires_from_now(boost::chrono::milliseconds(FPS_INTEGRATION_MS));
	fps_meter_timer.async_wait(boost::bind(&codec_processor::do_fps_meter,this,boost::asio::placeholders::error));


}

AVCodecID codec_processor::select_codec_for_container(const std::string& cont_name){


	AVCodecID c_id = AV_CODEC_ID_NONE;

	if ((cont_name=="mp4")||(cont_name=="avi")||(cont_name=="flv"))
		c_id = AV_CODEC_ID_H264;
	if (cont_name=="webm")
		c_id = AV_CODEC_ID_VP8;
	if (cont_name=="mjpeg")
		c_id = AV_CODEC_ID_MJPEG;

	return c_id;
}

void codec_processor::call_stop_handler(const std::string& reason){
	printf("---codec_processor::call_stop_handler called!\n");
	if (reason != ""){
		using namespace Utility;
		Journal::Instance()->Write(INFO,DST_STDOUT|DST_SYSLOG,"codec stopped:%s",reason.c_str());
	}
	_stop_handler(this);
}

void codec_processor::i_thread_frame_generator_cleanup(boost::system::error_code ec){
	if (ec)
		return ;
	_frame_generator.cleanup(boost::posix_time::seconds(FRAME_GENERATOR_CLEANUP_TIMEOUT));

	frame_generator_cleanup_timer.expires_from_now(boost::chrono::seconds(5));
	frame_generator_cleanup_timer.async_wait(boost::bind(&codec_processor::i_thread_frame_generator_cleanup,
		this,boost::asio::placeholders::error));

}
