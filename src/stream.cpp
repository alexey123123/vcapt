#include <boost/foreach.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <opencv2/opencv.hpp>

#include "stream.h"
#include "manager.h"

#define FPS_INTEGRATION_MS 3000

capturer::frame_ptr generate_blank_frame2(int width,int height);


stream::stream(manager* _m, camera* _c,boost::shared_ptr<ffmpeg_encoder> e_ptr, const parameters& p):
	_manager(_m),
	_camera(_c),
	_encoder_ptr(e_ptr),
	_parameters(p),
	work_timer(internal_ioservice),
	check_clients_timer(internal_ioservice),
	fps_meter_timer(internal_ioservice),
	encoded_frame_counter(0),


	//statistics
	encoder_fps(0),
	minimum_encode_time_ms(0xFFFFFFFF),
	maximum_encode_time_ms(0),
	summary_encode_time_ms(0)
	{
		std::cout<<"------stream constructor"<<std::endl;

	_encoder_ptr->initialize(_parameters.encoder_format,
		boost::bind(&stream::encoder_data_handler,this,_1));



	work_timer.expires_from_now(boost::chrono::milliseconds(50));
	work_timer.async_wait(boost::bind(&stream::do_stream_work,this,boost::asio::placeholders::error));

	fps_meter_timer.expires_from_now(boost::chrono::milliseconds(FPS_INTEGRATION_MS));
	fps_meter_timer.async_wait(boost::bind(&stream::do_fps_meter,this,boost::asio::placeholders::error));

	last_frame_tp = boost::chrono::steady_clock::now();


	//starting thread
	boost::system::error_code ec;
	internal_thread = boost::thread(boost::bind(&stream::internal_thread_proc,this));
}

stream::~stream(){
	


	try{

		boost::system::error_code ec;
		work_timer.cancel(ec);
		check_clients_timer.cancel(ec);
		fps_meter_timer.cancel(ec);


		internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>();

		std::cout<<"joining stream thread ("<<_parameters.encoder_format.fsize.to_string()<<")..."<<std::endl;
		internal_thread.join();
		std::cout<<"ok"<<std::endl;

	}
	catch(...){
		Utility::Journal::Instance()->Write(Utility::ALERT,Utility::DST_SYSLOG|Utility::DST_STDERR,"stream thread stop error");
	}

	std::cout<<"deleting encoder..."<<std::endl;
	_encoder_ptr = boost::shared_ptr<ffmpeg_encoder>();

	boost::unique_lock<boost::mutex> l1(clients_mutex);
	clients.clear();
	l1.unlock();

	std::cout<<"stream finished"<<std::endl;


}


void stream::internal_thread_proc(){

	try{
		internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>(
			new boost::asio::io_service::work(internal_ioservice));
		internal_ioservice.run();
	}
	catch(...){

	}
}

#define MAX_STREAM_EXCEPTIONS_COUNT 10

void stream::do_stream_work(boost::system::error_code ec){
	if (ec)
		return ;

	if (!internal_ioservice_work_ptr)
		return ;


	int work_interval_ms = 30;
	int exceptions_count = 0;
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
		
		_encoder_ptr->process_frame(fptr);
		last_frame_tp = fptr->tp;				

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
			Journal::Instance()->Write(ALERT,DST_SYSLOG|DST_STDERR,"stream thread exceeded exception limit");
			//finalize stream
			_manager->finalize_stream(this);

			return ;
		}
	}
	catch(...){
		//fatal exception
		using namespace Utility;
		Journal::Instance()->Write(ALERT,DST_SYSLOG|DST_STDERR,"stream thread stopped by unknown exception");

		//finalize stream
		_manager->finalize_stream(this);

		return ;
	}

	work_timer.expires_from_now(boost::chrono::milliseconds(work_interval_ms));
	work_timer.async_wait(boost::bind(&stream::do_stream_work,this,boost::asio::placeholders::error));
}


void stream::add_new_tcp_client(tcp_client_ptr c){
	internal_ioservice.post(boost::bind(&stream::i_thread_add_new_tcp_client,this,c));
}

void stream::remove_tcp_client(tcp_client_ptr c){
	internal_ioservice.post(boost::bind(&stream::i_thread_remove_tcp_client,this,c));
}

void stream::encoder_data_handler(buffer_ptr b){
	boost::unique_lock<boost::mutex> l1(clients_mutex);
	client_ptr ncptr;
	BOOST_FOREACH(ncptr,clients){
		if (!ncptr->header_sended){
			 	//header buffer
			 	buffer_ptr hb = _encoder_ptr->get_header();
				if (hb){
					ncptr->add_buffer(hb);
			 	}
				ncptr->header_sended = true;

		}
		ncptr->add_buffer(b);
		ncptr->send_first_buffer();
	}
}

void stream::i_thread_add_new_tcp_client(tcp_client_ptr c){

	boost::unique_lock<boost::mutex> l1(clients_mutex);

	client_ptr cptr(new client(this,c));
	clients.push_back(cptr);


	//HTTP answer
	buffer_ptr http_b = _encoder_ptr->get_http_ok_answer();
	if (http_b)
		cptr->add_buffer(http_b);

// 	//header buffer
// 	buffer_ptr hb = _encoder_ptr->get_header();
// 	if (hb){
// 		std::cout<<"header sended"<<std::endl;
// 		cptr->add_buffer(hb);
// 	}
	cptr->send_first_buffer();


	std::cout<< "i_thread_add_new_tcp_client ok" <<std::endl;
}

void stream::i_thread_remove_tcp_client(tcp_client_ptr c){
	boost::unique_lock<boost::mutex> l1(clients_mutex);
	
	client_ptr ncptr;
	std::deque<client_ptr> new_network_clients;

	BOOST_FOREACH(ncptr,clients)
		if (ncptr->_tcp_client_ptr.get() != c.get())
			new_network_clients.push_back(ncptr);
	clients = new_network_clients;

	check_clients_timer.expires_from_now(boost::chrono::seconds(2));
	check_clients_timer.async_wait(boost::bind(&stream::check_clients_and_finalize_stream,this,boost::asio::placeholders::error));

	std::cout<< "i_thread_remove_tcp_client ok" <<std::endl;

}


stream::client::~client(){

	buffers_mutex.lock();
	while(buffers.size()>0)
		buffers.pop();
	buffers_mutex.unlock();
	while (sent_packets_count>0)
		boost::this_thread::sleep(boost::posix_time::milliseconds(10));
}

#define MAX_CLIENT_BUFFER_SIZE 1000000


void stream::client::add_buffer(buffer_ptr b){
	boost::unique_lock<boost::mutex> l1(buffers_mutex);
	if (client_error_code)
		return ;
	while (total_size + b->size() >= MAX_CLIENT_BUFFER_SIZE){
		std::cout<<"add_buffer: need cleanup"<<std::endl;
		buffer_ptr b1 = buffers.front();
		if (b1){
			buffers.pop();
			total_size -= b1->size();
		} else
			break;
	}

	if (total_size + b->size() >= MAX_CLIENT_BUFFER_SIZE)
		throw std::runtime_error("NetworkStreamer: buffer overflow");

	buffers.push(b);
	total_size += b->size();
	// std::cout<<"B: total_size="<<total_size<<",count="<<buffers.size()<<std::endl;
}
void stream::client::send_first_buffer(){
	boost::unique_lock<boost::mutex> l1(buffers_mutex);
	if (!current_buffer){

		if (buffers.size()>0){
			current_buffer = buffers.front();
			buffers.pop();
		}

		if (current_buffer){
			boost::asio::async_write(_tcp_client_ptr->get_socket(),
				boost::asio::buffer(current_buffer->data(),current_buffer->size()),
				boost::bind(&stream::client::write_data_handler, 
				this,
				current_buffer,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
			sent_packets_count++;
		}

	}

}
void stream::client::write_data_handler(buffer_ptr bptr, boost::system::error_code ec,unsigned int bytes_transferred){
	boost::unique_lock<boost::mutex> l1(buffers_mutex);
	sent_packets_count--;
	if (!ec){

		if (bptr->size() != bytes_transferred)
			std::cout<<"------------TRANSFERED only "<<bytes_transferred<<" bytes from "<<bptr->size()<<std::endl;

		//send next data buffer

		if (current_buffer){
			total_size -= current_buffer->size();
			current_buffer = buffer_ptr();
		}
		l1.unlock();
		send_first_buffer();

	} else{
		//deleting client
		std::string e_mess = ec.message();
		std::cout<<"delivery error (bt:"<<bytes_transferred<<")("<<e_mess<<")"<<std::endl;
		client_error_code = ec;
		_stream->remove_tcp_client(_tcp_client_ptr);
	}

}

bool stream::have_priority_clients(){
	boost::unique_lock<boost::mutex> l1(clients_mutex);
	BOOST_FOREACH(client_ptr c,clients){
		if (c->_tcp_client_ptr->priority)
			return true;
	}
	return false;
}

bool stream::have_clients(){
	boost::unique_lock<boost::mutex> l1(clients_mutex);
	return clients.size() > 0;
}

void stream::check_clients_and_finalize_stream(boost::system::error_code ec){
	boost::unique_lock<boost::mutex> l1(clients_mutex);
	if (clients.size()==0){
		l1.unlock();
		_manager->finalize_stream(this);
	}
		
}

void stream::do_fps_meter(boost::system::error_code ec){
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
		std::cout<<"Encoder: encode fps changed: "<<encoder_fps<<"(avg time:"<<avg_time<<" ms,min:"<<minimum_encode_time_ms<<",max:"<<maximum_encode_time_ms<<")"<<std::endl;


		//TODO: filter rebuild (if need display fps)

	}	
	encoded_frame_counter = 0;

	fps_meter_timer.expires_from_now(boost::chrono::milliseconds(FPS_INTEGRATION_MS));
	fps_meter_timer.async_wait(boost::bind(&stream::do_fps_meter,this,boost::asio::placeholders::error));

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


		c =  libav::sws_getContext( cv_mat->cols,cv_mat->rows, 
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

