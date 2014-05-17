#include <boost/foreach.hpp>
#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>

#include "codec_processor.h"

#define FPS_INTEGRATION_MS 3000

capturer::frame_ptr generate_blank_frame2(int width,int height);

codec_processor::codec_processor(camera* _cam, codec_ptr _cod,stop_handler _sh):
	_camera(_cam),_codec(_cod),_stop_handler(_sh),
	work_timer(internal_ioservice),
	check_clients_timer(internal_ioservice),
	fps_meter_timer(internal_ioservice),
	summary_encode_time_ms(0),
	maximum_encode_time_ms(0),
	minimum_encode_time_ms(0xFFFFFFFF),
	encoded_frame_counter(0),
	encoder_fps(0),
	exceptions_count(0){
	
		work_timer.expires_from_now(boost::chrono::milliseconds(50));
		work_timer.async_wait(boost::bind(&codec_processor::do_processor_work,this,boost::asio::placeholders::error));

		fps_meter_timer.expires_from_now(boost::chrono::milliseconds(FPS_INTEGRATION_MS));
		fps_meter_timer.async_wait(boost::bind(&codec_processor::do_fps_meter,this,boost::asio::placeholders::error));

		last_frame_tp = boost::chrono::steady_clock::now();


	internal_thread = boost::thread(boost::bind(&codec_processor::internal_thread_proc,this));
}


codec_processor::~codec_processor(){
	std::cout<<"~codec_processor()"<<std::endl;
	internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>();

	work_timer.cancel();
	check_clients_timer.cancel();
	fps_meter_timer.cancel();
	
	
	internal_thread.join();
}

#define MAX_STREAM_EXCEPTIONS_COUNT 10

void codec_processor::do_processor_work(boost::system::error_code ec){
		if (ec)
			return ;

		if (!internal_ioservice_work_ptr)
			return ;


		int work_interval_ms = 30;
		try{
			using namespace boost::chrono;
			high_resolution_clock::time_point start = high_resolution_clock::now();

			//capture frame
			capturer::frame_ptr fptr = _camera->get_frame(last_frame_tp);
			capturer::state st = fptr->capturer_state;

			switch(st){
			case capturer::st_Ready:{
				break;
									}
			default:{
				//need generate state-picture
				if (!blank_frame){
					blank_frame = generate_blank_frame2(_camera->get_current_framesize().width,_camera->get_current_framesize().height);
					if (!blank_frame)
						throw std::runtime_error("cannot generate blank frame");
				}
				fptr = blank_frame;
				fptr->tp = boost::chrono::steady_clock::now();
				fptr->capturer_state = st;

				work_interval_ms = 300;
				break;

					}
			}
			
			

			packet_ptr pptr = _codec->process_frame(fptr);
			if (pptr){
				pptr->frame_tp = fptr->tp;

				last_frame_tp = fptr->tp;	


				//transfer packet to streams

				boost::unique_lock<boost::mutex> l1(internal_mutex);
				BOOST_FOREACH(stream_ptr sptr, streams){
					try{
						sptr->process_packet(pptr);
					}
					catch(...){
						std::cout<<"stream fail"<<std::endl;
						internal_ioservice.post(boost::bind(&codec_processor::i_thread_delete_stream,this,sptr.get()));
					}

				}
				l1.unlock();
			
			}


			//some statistics
			high_resolution_clock::time_point stop = high_resolution_clock::now();
			encoded_frame_counter++;


			uint64_t enc_duration = duration_cast<milliseconds> (stop - start).count();

			summary_encode_time_ms += enc_duration;
			if (enc_duration > maximum_encode_time_ms)
				maximum_encode_time_ms += enc_duration;
			if (enc_duration < minimum_encode_time_ms)
				minimum_encode_time_ms = enc_duration;

			exceptions_count = 0;
		}
		catch(std::runtime_error& ex){
			exceptions_count++;

			if (exceptions_count > MAX_STREAM_EXCEPTIONS_COUNT){
				using namespace Utility;
				Journal::Instance()->Write(ALERT,DST_SYSLOG|DST_STDERR,"codec thread exceeded exception limit");
				//finalize stream
				_stop_handler(this);

				return ;
			}
		}
		catch(...){
			//fatal exception
			using namespace Utility;
			Journal::Instance()->Write(ALERT,DST_SYSLOG|DST_STDERR,"codec thread stopped by unknown exception");

			//finalize stream
			_stop_handler(this);


			return ;
		}

		work_timer.expires_from_now(boost::chrono::milliseconds(work_interval_ms));
		work_timer.async_wait(boost::bind(&codec_processor::do_processor_work,this,boost::asio::placeholders::error));
	}



bool codec_processor::try_to_add_network_client(client_parameters e_params, tcp_client_ptr tcptr){
	if (e_params.codec_name != _codec->get_format().name)
		return false;

	if (e_params.f_size)
		if (*e_params.f_size != _codec->get_format().fsize)
			return false;

	if (e_params.bitrate)
		if (*e_params.bitrate != _codec->get_format().bitrate)
			return false;

	//create network stream
	stream_ptr sptr = av_container_stream::create_network_stream(e_params.container_name,_codec->get_avcodec_context(),
		boost::bind(&codec_processor::delete_stream,this,_1),
		tcptr);

	boost::unique_lock<boost::mutex> l1(internal_mutex);
	streams.push_back(sptr);

	return true;
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
	boost::unique_lock<boost::mutex> l1(internal_mutex);
	std::deque<stream_ptr> remained_streams;
	stream_ptr sptr;
	BOOST_FOREACH(sptr,streams)
		if (sptr.get() != s)
			remained_streams.push_back(sptr);
	sptr = stream_ptr();
	streams = remained_streams;

	if (streams.size()==0){
		check_clients_timer.expires_from_now(boost::chrono::milliseconds(2000));
		check_clients_timer.async_wait(boost::bind(&codec_processor::check_clients_and_finalize_processor,this,boost::asio::placeholders::error));

	}

}







AVFrame* convert_cv_Mat_to_avframe_yuv420p(cv::Mat* cv_mat,int dst_width,int dst_height,std::string& error_message){
	SwsContext* c = 0;
	bool ret = true;
	AVFrame* ret_frame = 0;

	AVPicture pic_bgr24;
	try{
		//1. Convert cv::Mat to AVPicture
		std::vector<uint8_t> buf;
		buf.resize(cv_mat->cols * cv_mat->rows * 3); // 3 bytes per pixel
		for (int i = 0; i < cv_mat->rows; i++)
		{
			memcpy( &( buf[ i*cv_mat->cols*3 ] ), &( cv_mat->data[ i*cv_mat->step ] ), cv_mat->cols*3 );
		}

		libav::avpicture_fill(&pic_bgr24, &buf[0], AV_PIX_FMT_BGR24, cv_mat->cols,cv_mat->rows);
		//buf.clear();

		//2. BGR24 -> YUV420P


		c = libav::sws_getContext( cv_mat->cols,cv_mat->rows,
			AV_PIX_FMT_BGR24,
			dst_width, dst_height,AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL );
		if (!c)
			throw std::runtime_error("cannot allocate SwsContext");

		int res=-55;


		//ret_value = Frame::construct_and_alloc(dst_pixel_format,src->avframe()->width, src->avframe()->height,1);


		ret_frame = libav::av_frame_alloc();
		ret_frame->width = dst_width;
		ret_frame->height = dst_height;
		ret_frame->format = AV_PIX_FMT_YUV420P;
		libav::av_frame_get_buffer(ret_frame,1);

		res = libav::sws_scale( c ,
			pic_bgr24.data, pic_bgr24.linesize,
			0,
			cv_mat->rows,
			ret_frame->data, ret_frame->linesize );

		ret_frame->format = AV_PIX_FMT_YUV420P;

	}
	catch(std::runtime_error& ex){
		error_message = std::string(ex.what());
		if (ret_frame != 0){
			libav::av_frame_free(&ret_frame);
		}
		ret_frame = 0;
	}


	if (c)
		libav::sws_freeContext(c);

	return ret_frame;
}
capturer::frame_ptr generate_blank_frame2(int width,int height){
	using namespace cv;

	int w = width;
	if (w==-1)
		w = 640;

	int h = height;
	if (h==-1)
		h = 480;

	cv::Mat m1(h, w, CV_8UC3, Scalar(0,0,0));

	capturer::frame_ptr fptr;

	std::string error_message;
	AVFrame* av_f1 = convert_cv_Mat_to_avframe_yuv420p(&m1,w,h,error_message);
	if (!av_f1)
		return fptr;

	fptr = capturer::frame_ptr(new capturer::frame());
	fptr->avframe = av_f1;


	return fptr;

}

void codec_processor::check_clients_and_finalize_processor(boost::system::error_code ec){
	if (ec)
		return ;
	if (!internal_ioservice_work_ptr)
		return ;

	boost::unique_lock<boost::mutex> l1(internal_mutex);
	if (streams.size()==0){
		l1.unlock();
		_stop_handler(this);
	}

}

void codec_processor::do_fps_meter(boost::system::error_code ec){
	if (ec)
		return ;

	if (!internal_ioservice_work_ptr)
		return ;

	uint64_t avg_time = -1;
	if (encoded_frame_counter > 0)
		avg_time = summary_encode_time_ms / encoded_frame_counter;
	summary_encode_time_ms = 0;

	int new_fps = encoded_frame_counter / (FPS_INTEGRATION_MS / 1000);
	if (encoder_fps != new_fps){
		encoder_fps = new_fps;
		std::cout<<"codec processor: encode fps changed: "<<encoder_fps<<"(avg time:"<<avg_time<<" ms,min:"<<minimum_encode_time_ms<<",max:"<<maximum_encode_time_ms<<")"<<std::endl;


		//TODO: filter rebuild (if need display fps)

	}	
	encoded_frame_counter = 0;

	fps_meter_timer.expires_from_now(boost::chrono::milliseconds(FPS_INTEGRATION_MS));
	fps_meter_timer.async_wait(boost::bind(&codec_processor::do_fps_meter,this,boost::asio::placeholders::error));


}