

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/time_formatters.hpp>
#include <boost/filesystem.hpp>
#include <boost/memory_order.hpp>

#include <system/Platform.h>

#include <utility/Sha.h>
#include <utility/Pack.h>

//#include "v4l2_specific.h"
#include "camera.h"



#define MAX_INIT_ATTEMPTS 3
#define INIT_RETRY_TIMEOUT_SEC 1
#define BUFFERS_COUNT 5


std::string __pack_framesizes_to_string__(const std::deque<frame_size>& fsizes);

camera::camera(const capturer::connect_parameters& _cp,state_change_handler _state_h, stop_handler _stop_h):
		conn_params(_cp),_state_change_handler(_state_h),_stop_handler(_stop_h),
		connect_camera_st(service_ioservice),
		connect_attempts_count(0),
		finalization(false),
		current_streaming_state(ss_stopped){

	journal = Utility::Journal::Instance();

	//start service thread
	service_thread = boost::thread(boost::bind(&camera::service_thread_proc,this));

	start_connection(1000);

}
camera::~camera(){
	finalize();
}

void camera::do_connect_camera_device(boost::system::error_code ec){
	if (ec)
		return ;

	if (!service_ioservice_work_ptr)
		return ;

	try{

		//connect
		connect(get_connect_parameters());

		connect_attempts_count = 0;

		return ;
	}
	catch(std::runtime_error& ex){
		connect_attempts_count++;
		std::cerr<<"connect error:"<<ex.what()<<std::endl;
	}

	if (!service_ioservice_work_ptr)
		return ;


	//connect again
	if (connect_attempts_count < conn_params.max_connect_attempts)
		start_connection(conn_params.connect_attempts_interval); else
		_stop_handler("connection attempts limit exceeded");

}
/*
void camera::do_connect_camera_device(boost::system::error_code ec){
	if (ec)
		return ;

	if (!service_ioservice_work_ptr)
		return ;

	try{
		
		std::string error_message;

		//prepare
		capturer::connect_parameters s;
		switch(_type){
			case c_local:{
				s.connection_string = url;
				break;
			}
			case c_network:{
				//get doc
				
				if (!main_doc){
					if (!check_and_create_documents2(cdb_manager,url,error_message))
						throw std::runtime_error("documents init error:"+error_message);

					main_doc->set_property<std::string>("name",get_definition().device_name);
					main_doc->set_property<std::string>("manufacturer",get_definition().manufacturer_name);

					camera_unique_id = url;
				}


				std::string url  = main_doc->get_property<std::string>(cprops::Url,"");
				if (url == "")
					throw std::runtime_error("url not defined");

				s.connection_string = url;

				break;
			}
		}

		s.maximum_buffer_size_mb = 20;


		//connect
		connect(s);
		if (capturer::get_state() != capturer::st_Ready)
			throw std::runtime_error("connect error");



		//post-connect (check/create documents)
		capturer::definition def = get_definition();

		

		switch(_type){
			case c_local:{
				std::cout<<"unique_id:"<<def.unique_string<<std::endl;

				//get HASH of unique-id
				camera_unique_id = Utility::Sha::calc((unsigned char*)def.unique_string.c_str(),
					def.unique_string.size());


				if (!check_and_create_documents2(cdb_manager,camera_unique_id,error_message))
					throw std::runtime_error("documents init error:"+error_message);

				main_doc->set_property<std::string>("name",def.device_name);
				main_doc->set_property<std::string>("manufacturer",def.manufacturer_name);

				break;
			}
			case c_network:{
				break;
			}
		}

		std::cout<<"camera:"<<camera_unique_id<<"connect ok"<<std::endl;
		std::cout<<"Device name:"<<def.device_name<<std::endl;
		std::cout<<"Manufacturer:"<<def.manufacturer_name<<std::endl;

		runtime_doc->set_property<std::string>(cprops::CameraState,"ready");


		//sync framesizes
		try{

			capturer::format f = get_current_format();
			std::deque<frame_size> poss_fsizes = f.get_possible_framesizes();
			main_doc->set_property<std::string>(cprops::PossibleFrameSizes,
				__pack_framesizes_to_string__(poss_fsizes));

			frame_size db_framesize;
			db_framesize.from_string(main_doc->get_property<std::string>(cprops::FrameSize,""),frame_size(640,480));

			//setup
			set_framesize(db_framesize);

			//update database
			main_doc->set_property<std::string>(cprops::FrameSize,db_framesize.to_string());

		}
		catch(std::runtime_error& ex){
			using namespace Utility;
			journal->Write(ERR,DST_STDERR|DST_SYSLOG,"framesize setup error:%s",ex.what());
			//TODO: state update
		}

		//TODO: get/set capture options

		return ;
	}
	catch(std::runtime_error& ex){

		std::cerr<<"connect error:"<<ex.what()<<std::endl;
	}

	if (!service_ioservice_work_ptr)
		return ;


	//connect again
	start_connection(1000);


}
*/
void camera::start_connection(unsigned int _delay_ms){
	connect_camera_st.expires_from_now(boost::chrono::milliseconds(_delay_ms));
	connect_camera_st.async_wait(boost::bind(&camera::do_connect_camera_device,this,boost::asio::placeholders::error));}




void camera::finalize(){
	finalization = true;

	service_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>();




	boost::system::error_code ec;
	connect_camera_st.cancel(ec);
	service_thread.join();	
}

/*
bool camera::check_and_create_documents2(couchdb::manager* dmanager,
	const std::string& camera_unique_id,
	std::string& error_message){


		//find/create main doc
		std::string main_doc_id;
		switch(_type){
		case c_local:{
			main_doc = find_local_camera_document(camera_unique_id,error_message);
			break;
					  }
		case c_network:{
			main_doc_id = camera_unique_id;
			main_doc = dmanager->get_document(main_doc_id,error_message);
						}
		}

		if (!main_doc){
			main_doc = dmanager->create_document(main_doc_id,error_message);
			if (!main_doc)
				return false;
			switch(_type){
			case c_local:
				main_doc->add_tag("local_camera");
				break;
			case c_network:
				main_doc->add_tag("network_camera");
				break;
			}			
		}
		main_doc->set_property<std::string>(cprops::UniqueId,camera_unique_id);


		//runtime doc
		std::string runtime_doc_id = main_doc->get_property<std::string>(cprops::RuntimeDocId,"");
		if (runtime_doc_id != ""){
			//need cleanup exist document
			runtime_doc = dmanager->get_document(runtime_doc_id,error_message);
			if (runtime_doc)
				runtime_doc->clear();

		}
		if (!runtime_doc){
			runtime_doc = dmanager->create_document(runtime_doc_id,error_message);
			if (!runtime_doc)
				return false;
			if (runtime_doc->id() != runtime_doc_id)
				main_doc->set_property<std::string>(cprops::RuntimeDocId, runtime_doc->id());
		}
		runtime_doc->add_tag("temp");



		//controls_doc
		std::string controls_doc_id = main_doc->get_property<std::string>(cprops::ControlsDocId,"");
		if (controls_doc_id != ""){
			controls_doc = dmanager->get_document(controls_doc_id,error_message);
		}
		if (!controls_doc){
			controls_doc_id = "";
			controls_doc = dmanager->create_document(controls_doc_id,error_message);
			if (!controls_doc)
				return false;
			controls_doc_id = controls_doc->id();

			main_doc->set_property<std::string>(cprops::ControlsDocId,controls_doc_id);
		}
		controls_doc->set_property_if_not_exists<int>(cprops::controls_RotateAngle,0);
		controls_doc->set_property_if_not_exists<bool>(cprops::controls_DisplayCameraName,false);
		controls_doc->set_property_if_not_exists<bool>(cprops::controls_WriteDateTimeText,false);
		controls_doc->set_property_if_not_exists<std::string>(cprops::controls_CameraName,"");
		std::cout<<"controls_doc_id="<<controls_doc->id()<<std::endl;

		runtime_doc_ticket = runtime_doc->set_change_handler(boost::bind(&camera::runtime_doc_changed,this,_1,_2,_3));
		controls_doc_ticket = controls_doc->set_change_handler(boost::bind(&camera::controls_doc_changed,this,_1,_2,_3));
		main_doc_ticket = main_doc->set_handlers(
			boost::bind(&camera::main_doc_changed,this,_1,_2,_3),
			boost::bind(&camera::main_doc_deleted,this));

		return true;
}
*/


void camera::service_thread_proc(){

	try{
		service_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>(
			new boost::asio::io_service::work(service_ioservice));
		service_ioservice.run();
	}
	catch(...){

	}
}






void camera::main_doc_deleted(){
	/*
	using namespace Utility;
	journal->Write(NOTICE,DST_SYSLOG|DST_STDOUT,"camera document erased. stop camera-object");
	//delete runtime document
	if (runtime_doc)
		cdb_manager->delete_document(runtime_doc);
	_stop_handler(camera_unique_id);
	*/
}


void camera::runtime_doc_changed(std::string doc_id,std::string property_name,std::string property_value){
	std::cout<<"runtime_doc_changed: "<<property_name<<"="<<property_value<<std::endl;

}

void camera::controls_doc_changed(std::string doc_id,std::string property_name,std::string property_value){
	std::cout<<"controls_doc_changed: "<<property_name<<"="<<property_value<<std::endl;

}







void camera::do_disconnect_camera_device(){
	disconnect();
	start_connection(100);
}



void camera::on_state_change(capturer::state old_state,capturer::state new_state){

	if (finalization)
		return ;

	_state_change_handler(old_state,new_state);

	if (old_state==capturer::st_Ready)
		if (new_state != st_Setup){
			//link error ?
			disconnect();
			start_connection(get_connect_parameters().connect_attempts_interval);
		}
}

/*
void camera::on_state_change(capturer::state old_state,capturer::state new_state){

	std::string doc_property_value = "";
	switch(new_state){
	case capturer::st_Initialization:
		doc_property_value = "initialization";
		break;
	case capturer::st_InitializationError:
		doc_property_value = "initialization error";
		break;
	case capturer::st_ConnectError:
		doc_property_value = "link error";
		break;
	case capturer::st_CaptureError:
		doc_property_value = "capture error";
		break;
	case capturer::st_Ready:
		doc_property_value = "ready";
		break;
	}

	if (runtime_doc)
		runtime_doc->set_property<std::string>(cprops::CameraState,doc_property_value);

	if (_type==c_network)



	



}


*/



void camera::do_change_framesize(frame_size fs){
	std::cout<<"framesize change event ("<<fs.to_string()<<")"<<std::endl;
	set_framesize(fs);
}

void camera::change_streaming_state(streaming_state ss){
	if (!get_capabilities().start_stop_streaming_supported())
		return ;
	if (ss==current_streaming_state)
		return ;

	switch(ss){
		case ss_started:
			start_streaming();
			break;
		case ss_stopped:
			stop_streaming();
			break;
	}

	current_streaming_state = ss;
}