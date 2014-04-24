

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

#if defined(LinuxPlatform)
const std::string FontsPath = "/usr/local/share/fonts/";
#elif defined(Win32Platform)
const std::string FontsPath = "";
#endif


camera::camera(type t, const std::string& _dev_name_or_doc_id, couchdb::manager* _cdb_manager,stop_handler _h):
		_type(t),
		dev_name_or_doc_id(_dev_name_or_doc_id),
			cdb_manager(_cdb_manager),
			_stop_handler(_h),
		connect_camera_st(service_ioservice){

	journal = Utility::Journal::Instance();

	//start service thread
	service_thread = boost::thread(boost::bind(&camera::service_thread_proc,this));


}
camera::~camera(){
	finalize();
}

void camera::do_connect_camera_device(boost::system::error_code ec){
	if (ec)
		return ;

	if (!capture_ioservice_work_ptr)
		return ;

	try{
		
		std::string error_message;

		//prepare
		capturer::connect_parameters s;
		switch(_type){
			case c_local:{
				s.connection_string = dev_name_or_doc_id;
				break;
			}
			case c_network:{
				//get doc
				
				if (!main_doc){
					if (!check_and_create_documents2(cdb_manager,dev_name_or_doc_id,error_message))
						throw std::runtime_error("documents init error:"+error_message);

					main_doc->set_property<std::string>("name",get_definition().device_name);
					main_doc->set_property<std::string>("manufacturer",get_definition().manufacturer_name);

					camera_unique_id = dev_name_or_doc_id;
				}


				std::string url  = main_doc->get_property<std::string>(CameraDocumentProps::Url,"");
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

		runtime_doc->set_property<std::string>(CameraDocumentProps::CameraState,"ready");

		//TODO: get/set capture options

		return ;
	}
	catch(std::runtime_error& ex){
		std::cerr<<"connect error:"<<ex.what()<<std::endl;
	}

	if (!capture_ioservice_work_ptr)
		return ;


	//connect again
	start_connection(1000);


}

void camera::start_connection(unsigned int _delay_ms){
	connect_camera_st.expires_from_now(boost::chrono::milliseconds(_delay_ms));
	connect_camera_st.async_wait(boost::bind(&camera::do_connect_camera_device,this,boost::asio::placeholders::error));}




void camera::finalize(){




	main_doc_ticket = couchdb::document::ticket_ptr();
	runtime_doc_ticket = couchdb::document::ticket_ptr();
	controls_doc_ticket = couchdb::document::ticket_ptr();


	capture_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>();




	boost::system::error_code ec;
	connect_camera_st.cancel(ec);
	service_thread.join();	
}
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
		main_doc->set_property<std::string>(CameraDocumentProps::UniqueId,camera_unique_id);


		//runtime doc
		std::string runtime_doc_id = main_doc->get_property<std::string>(CameraDocumentProps::RuntimeDocId,"");
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
				main_doc->set_property<std::string>(CameraDocumentProps::RuntimeDocId, runtime_doc->id());
		}
		runtime_doc->add_tag("temp");



		//controls_doc
		std::string controls_doc_id = main_doc->get_property<std::string>(CameraDocumentProps::ControlsDocId,"");
		if (controls_doc_id != ""){
			controls_doc = dmanager->get_document(controls_doc_id,error_message);
		}
		if (!controls_doc){
			controls_doc_id = "";
			controls_doc = dmanager->create_document(controls_doc_id,error_message);
			if (!controls_doc)
				return false;
			controls_doc_id = controls_doc->id();

			main_doc->set_property<std::string>(CameraDocumentProps::ControlsDocId,controls_doc_id);
		}
		controls_doc->set_property_if_not_exists<int>(CameraDocumentProps::controls_RotateAngle,0);
		controls_doc->set_property_if_not_exists<bool>(CameraDocumentProps::controls_DisplayCameraName,false);
		controls_doc->set_property_if_not_exists<bool>(CameraDocumentProps::controls_WriteDateTimeText,false);
		controls_doc->set_property_if_not_exists<std::string>(CameraDocumentProps::controls_CameraName,"");
		std::cout<<"controls_doc_id="<<controls_doc->id()<<std::endl;

		runtime_doc_ticket = runtime_doc->set_change_handler(boost::bind(&camera::runtime_doc_changed,this,_1,_2,_3));
		controls_doc_ticket = controls_doc->set_change_handler(boost::bind(&camera::controls_doc_changed,this,_1,_2,_3));
		main_doc_ticket = main_doc->set_handlers(
			boost::bind(&camera::main_doc_changed,this,_1,_2,_3),
			boost::bind(&camera::main_doc_deleted,this));

		return true;
}


couchdb::document_ptr camera::find_local_camera_document(const std::string& camera_id,std::string& error_message){
	//get list of local-cameras
	boost::property_tree::ptree cameras_ptree;

	if (!cdb_manager->load_ptree_document("_design/vcapt/_view/local_cameras",cameras_ptree,error_message))
		throw std::runtime_error("_design/vcapt/_view/local_cameras error:"+error_message+" (db no initialized?)");

	boost::optional<int> cameras_count = cameras_ptree.get_optional<int>("total_rows");
	if (!cameras_count)
		throw std::runtime_error("couch-db incorrect answer: total_rows missed");
	if (cameras_count)
		if ((*cameras_count)>0)
			BOOST_FOREACH(boost::property_tree::ptree::value_type &v,cameras_ptree.get_child("rows")){
				boost::property_tree::ptree pt1 = v.second;
				boost::property_tree::ptree cam_ptree = pt1.get_child("value",boost::property_tree::ptree());

				std::string cam_unique_id = cam_ptree.get(CameraDocumentProps::UniqueId,"");
				if (cam_unique_id == camera_id){
					std::string doc_id = cam_ptree.get("_id","");
					return cdb_manager->get_document(doc_id,error_message);
				}
		}
	return couchdb::document_ptr();
}


void camera::service_thread_proc(){

	try{
		capture_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>(
			new boost::asio::io_service::work(service_ioservice));
		service_ioservice.run();
	}
	catch(...){

	}
}





void camera::main_doc_changed(std::string doc_id,std::string property_name,std::string property_value){
	std::cout<<"main_doc_changed: "<<property_name<<"="<<property_value<<std::endl;
	using namespace Utility;
	if (property_name==CameraDocumentProps::Url)
		if (_type==c_network){
			service_ioservice.post(boost::bind(&camera::do_disconnect_camera_device,this));
		}
}

void camera::main_doc_deleted(){
	using namespace Utility;
	journal->Write(NOTICE,DST_SYSLOG|DST_STDOUT,"camera document erased. stop camera-object");
	//delete runtime document
	if (runtime_doc)
		cdb_manager->delete_document(runtime_doc);
	_stop_handler(camera_unique_id);
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
		runtime_doc->set_property<std::string>(CameraDocumentProps::CameraState,doc_property_value);

	if (_type==c_network)
		if (old_state==capturer::st_Ready){
			//link error ?
			disconnect();
			start_connection(1000);
		}


	



}

