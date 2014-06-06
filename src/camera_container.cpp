#include <boost/foreach.hpp>

#include "couchdb.h"
#include "camera_container.h"
#include "camera_opencv.h"
#include "camera_v4l2.h"




camera_container::camera_container(couchdb::manager* _cdb_manager, couchdb::document_ptr d, stop_handler sh):
					cdb_manager(_cdb_manager),main_doc(d),_stop_handler(sh){

	std::string error_message;

	//create runtime document
	std::string runtime_doc_id = main_doc->get_property<std::string>(cprops::RuntimeDocId,"");
	if (runtime_doc_id != ""){
		//need cleanup exist document
		runtime_doc = cdb_manager->get_document(runtime_doc_id,error_message);
		if (runtime_doc)
			runtime_doc->clear();

	}
	if (!runtime_doc){
		runtime_doc = cdb_manager->create_document(runtime_doc_id,error_message);
		if (!runtime_doc)
			throw std::runtime_error("cannot create runtime doc:"+error_message);
		if (runtime_doc->id() != runtime_doc_id)
			main_doc->set_property<std::string>(cprops::RuntimeDocId, runtime_doc->id());
	}
	runtime_doc->add_tag("temp");


	std::string d_id = main_doc->get_property<std::string>(cprops::RuntimeDocId,"");
	if (!streams_doc){

	}




	//what'a a camera ?
	_camera = 0;


	capturer::connect_parameters cp;
	cp.maximum_buffer_size_mb = 10;
	//TODO: get connect parameters from document
	cp.connection_string = main_doc->get_property<std::string>(cprops::Url,"");

	if ((cp.connection_string==std::string(WindowsCameraDevname))||(main_doc->have_tag(cprops::NetworkCameraTag))){
		//it is a local windows cam or http
		cp.connect_attempts_interval = 1000;
		cp.max_connect_attempts = 0xFFFFFFF;
		_camera = new camera_opencv(cp,
			boost::bind(&camera_container::camera_state_change_handler,this,_1,_2), 
			boost::bind(&camera_container::stop_camera_handler,this,_1));
	} else
	if (main_doc->have_tag(cprops::LocalCameraTag)){
		//v4l2
		cp.connect_attempts_interval = 0;
		cp.max_connect_attempts = 1;
		_camera = new camera_v4l2(cp,
			boost::bind(&camera_container::camera_state_change_handler,this,_1,_2), 
			boost::bind(&camera_container::stop_camera_handler,this,_1));

	}
	
	main_doc->set_property<int>(cprops::CameraState,_camera->get_state());

	main_doc_tptr = main_doc->set_handlers(
		boost::bind(&camera_container::camera_main_doc_changed,this,_1,_2,_3),
		boost::bind(&camera_container::camera_main_doc_deleted,this));


	internal_thread = boost::thread(boost::bind(&camera_container::internal_thread_proc,this));
}

camera_container::~camera_container(){

	main_doc_tptr = couchdb::document::ticket_ptr();
	codec_processors.clear();
	delete _camera;
	internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>();	
	internal_thread.join();
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

void camera_container::i_thread_dispatch_url_change(std::string new_url){
	if (main_doc->have_tag(cprops::NetworkCameraTag)){
		using namespace Utility;
		Journal::Instance()->Write(NOTICE,DST_SYSLOG|DST_STDERR,"camera:%s: url changed (%s)",
			main_doc->id().c_str(),
			new_url.c_str());
		capturer::connect_parameters cp = _camera->get_connect_parameters();
		cp.connection_string = new_url;
		_camera->set_connect_parameters(cp);
		_camera->restart_connection();
	}

}

void camera_container::camera_main_doc_changed(std::string doc_id,std::string property_name,std::string property_value){
	using namespace Utility;
	std::cout<<"main_doc_changed: "<<property_name<<"="<<property_value<<std::endl;
	using namespace Utility;
	if (property_name==cprops::Url)
		internal_ioservice.post(boost::bind(&camera_container::i_thread_dispatch_url_change,this,property_value));

	if (property_name==cprops::FrameSize){
			frame_size fs;
			fs.from_string(property_value, frame_size());
			if (fs==frame_size()){
				using namespace Utility;
				Journal::Instance()->Write(NOTICE,DST_SYSLOG|DST_STDERR,"camera:%s: cannot decode framesize: %s",
					main_doc->id().c_str(),
					property_value.c_str());
				return ;
			}
			if (fs == _camera->get_current_framesize())
				return ;

			//is a framesize possible ?
			capturer::format c_f = _camera->get_current_format();
			if (!c_f.check_framesize(fs)){
				using namespace Utility;
				Journal::Instance()->Write(NOTICE,DST_SYSLOG|DST_STDERR,"camera:%s: cannot set specified framesize: %s",
					main_doc->id().c_str(),
					fs.to_string().c_str());
				return ;
			}


			internal_ioservice.post(boost::bind(&camera::set_framesize,_camera,fs));
		}

}
void camera_container::camera_main_doc_deleted(){

}

void camera_container::stop_camera_handler(std::string stop_reason){
	//TODO: main_doc
	using namespace Utility;

	Journal::Instance()->Write(NOTICE,DST_SYSLOG|DST_STDOUT,"camera %s stopped. reason:%s",
		main_doc->id().c_str(),
		stop_reason.c_str());

	_stop_handler(this);
}


std::string __pack_framesizes_to_string__(const std::deque<frame_size>& fsizes);

void camera_container::camera_state_change_handler(capturer::state old_state,capturer::state new_state){
	if (runtime_doc)
		runtime_doc->set_property<int>(cprops::CameraState, new_state);
	using namespace Utility;
	Journal::Instance()->Write(DEB,DST_STDOUT|DST_SYSLOG,"cam:%s: state change: %d -> %d",
		main_doc->id().c_str(),(int)old_state,(int)new_state);

	if (new_state == capturer::st_Ready){
		//fill properties
		capturer::definition def = _camera->get_definition();

		main_doc->set_property<std::string>("name",def.device_name);
		main_doc->set_property<std::string>("manufacturer",def.manufacturer_name);

		
		//sync framesizes
		try{

			capturer::format f = _camera->get_current_format();
			std::deque<frame_size> poss_fsizes = f.get_possible_framesizes();
			main_doc->set_property<std::string>(cprops::PossibleFrameSizes,
				__pack_framesizes_to_string__(poss_fsizes));

			frame_size db_framesize;
			db_framesize.from_string(main_doc->get_property<std::string>(cprops::FrameSize,""),frame_size(640,480));

			//setup
			frame_size c_fs = _camera->get_current_framesize();
			if (c_fs != db_framesize)
				_camera->set_framesize(db_framesize);

			//update database
			main_doc->set_property<std::string>(cprops::FrameSize,db_framesize.to_string());
		}
		catch(std::runtime_error& ex){
			using namespace Utility;
			Journal::Instance()->Write(ERR,DST_STDERR|DST_SYSLOG,"camera:%s: cannot sync framesize:%s",
				main_doc->id().c_str(),
				ex.what());
		}

		//TODO: sync controls

		//any streams?




	}

}



//320x200;640x480;800x600
std::string __pack_framesizes_to_string__(const std::deque<frame_size>& fsizes){
	std::ostringstream oss;
	int count = 0;
	BOOST_FOREACH(frame_size f,fsizes){
		if (count != 0)
			oss << ";";
		oss<<f.width<<"x"<<f.height;
		count++;
	}
	return oss.str();
}


