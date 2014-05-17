#include <boost/foreach.hpp>

#include "camera_container.h"




camera_container::camera_container(camera* c):_camera(c){


	internal_thread = boost::thread(boost::bind(&camera_container::internal_thread_proc,this));
}

camera_container::~camera_container(){

	internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>();

	codec_processors.clear();

	
	internal_thread.join();


	delete _camera;
}


void camera_container::add_network_client(tcp_client_ptr c){
	client_parameters e_params;
	std::string error_message;
	if (!e_params.construct(c,error_message))
		throw std::runtime_error("HTTP/1.0 400 bad request:"+error_message);

	//DEBUG
// 	if (!c->internal_client){
// 		e_params.f_size = boost::optional<frame_size>();
// 		e_params.bitrate = boost::optional<int>();
// 	}
		

	codec_processor_ptr cptr;
	//1. try to find similar processor (equal framesize&bitrate)
	BOOST_FOREACH(cptr,codec_processors){
		if (cptr->try_to_add_network_client(e_params,c))
			return ;

	}

	//2. Framesize check
	//(system clients may downscale the framesize)
	//TODO: 
	// 	if (c->internal_client)
	// 		if (e_params.f_size)
	// 			if (*e_params.f_size) >
	// 

	//2. try to create separate processor (and codec)
	codec_ptr new_codec;
	try{
		codec::format c_f;
		c_f.name = e_params.codec_name;

		c_f.fsize = _camera->get_current_framesize();
		if (e_params.f_size)
			c_f.fsize = *(e_params.f_size);

		c_f.bitrate = 500000;
		if (e_params.bitrate)
			c_f.bitrate = *e_params.bitrate;

		c_f.input_pixfmt = _camera->get_current_format().ffmpeg_pixfmt;


		new_codec = codec::create(c_f);


	}
	catch(codec::ex_codecs_limit_overflow){
		//TODO:
		//need erase existing codec processor with low priority
		throw std::runtime_error("HTTP/1.0 400 codecs limit overflowed"); 
	}

	codec_processor_ptr proc_ptr(new codec_processor(_camera,new_codec,
		boost::bind(&camera_container::delete_codec_processor,this,_1)));

	proc_ptr->try_to_add_network_client(e_params,c);
	
	boost::unique_lock<boost::mutex> l1(internal_mutex);
	codec_processors.push_back(proc_ptr);
}

void camera_container::delete_codec_processor(codec_processor* p){
	internal_ioservice.post(boost::bind(&camera_container::i_thread_delete_codec_processor,this,p));
}

void camera_container::i_thread_delete_codec_processor(codec_processor* cp){
	
	boost::unique_lock<boost::mutex> l1(internal_mutex);

	std::deque<codec_processor_ptr> remained_processors;
	codec_processor_ptr cptr;
	BOOST_FOREACH(cptr,codec_processors)
		if (cptr.get() != cp)
			remained_processors.push_back(cptr);

	cptr = codec_processor_ptr();

	codec_processors = remained_processors;
}


void camera_container::internal_thread_proc(){
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

