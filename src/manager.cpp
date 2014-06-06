#include <boost/filesystem.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/foreach.hpp>
#include <boost/bind.hpp>

#include <system/Platform.h>

#include <utility/Sha.h>

#include "manager.h"
#include "camera.h"

#include "camera_opencv.h"

const unsigned short UserTcpPortNum	= 5089;
const unsigned short SystemTcpPortNum = 5090;



manager::manager(Utility::Options* options):
	cdb_manager(options->conf_connection,options->conf_dbname),
	local_devices_scan_timer(internal_ioservice),
	u_acceptor(internal_ioservice),
	s_acceptor(internal_ioservice)
{


	//check&create database views
	check_database_and_start_network_cams();


	cdb_manager.set_additional_change_handler(boost::bind(&manager::network_camera_doc_changed,this,_1,_2));

	cdb_manager.Start();
	



	//local devices scanner
	//ugly&fast solution
	videodevices["/dev/video0"] = 0;
	videodevices["/dev/video1"] = 0;
	videodevices["/dev/video2"] = 0;
	videodevices["/dev/video3"] = 0;
	videodevices["/dev/video4"] = 0;
	videodevices["/dev/video5"] = 0;
	//for win32 debug
	videodevices[std::string(WindowsCameraDevname)] = 0;


	local_devices_scan_timer.expires_from_now(boost::chrono::milliseconds(1000));
	local_devices_scan_timer.async_wait(boost::bind(&manager::do_local_devices_scan,this,boost::asio::placeholders::error));


	//acceptors
	start_acceptor(u_acceptor,UserTcpPortNum);
	start_acceptor(s_acceptor,SystemTcpPortNum);

	//starting thread
	boost::system::error_code ec;
	internal_thread = boost::thread(boost::bind(&manager::internal_thread_proc,this));

}
manager::~manager(){


	internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>();

	boost::system::error_code ec;
	local_devices_scan_timer.cancel(ec);

	u_acceptor.close();
	s_acceptor.close();


	cameras.clear();

	internal_thread.join();
}



void manager::network_camera_doc_changed(const std::string& doc_id, const boost::property_tree::ptree& document_ptree){
	std::string device_tags = document_ptree.get<std::string>("device_tags","");
	if (device_tags.find(cprops::NetworkCameraTag))
		internal_ioservice.post(boost::bind(&manager::i_thread_network_camera_doc_changed,this,doc_id,document_ptree));


}
void manager::i_thread_network_camera_doc_changed(std::string doc_id, boost::property_tree::ptree document_ptree){
	std::cout<<"network_camera_doc_changed ("<<doc_id<<")"<<std::endl;
	camera_container_ptr cc_ptr;
	BOOST_FOREACH(cc_ptr,cameras)
		if (cc_ptr->camera_id() == doc_id)
			return ;
	std::string error_message;
	couchdb::document_ptr d = cdb_manager.get_document(doc_id,error_message);
	if (d){
		try{
			camera_container_ptr cptr(new camera_container(&cdb_manager, d,boost::bind(&manager::camera_container_stop_handler,this,_1)));
			cameras.push_back(cptr);
		}
		catch(std::runtime_error& ex){
			using namespace Utility;
			Journal::Instance()->Write(ERR,DST_STDERR|DST_SYSLOG,"cannot start network cam (id:%s): %s",
				d->id().c_str(),
				ex.what());

		}

	}
}


void manager::check_database_and_start_network_cams(){
			std::string error_message;

		//initialize views
		
		std::cout<<"update vcapt-views in couchdb...";
		boost::property_tree::ptree views_pt;
		if (cdb_manager.load_ptree_document("_design/vcapt",views_pt,error_message)){

			if (!cdb_manager.delete_ptree_document(views_pt,error_message))
				throw std::runtime_error("cannot delete _design/vcapt:"+error_message);

		}
			

		views_pt.clear();
		views_pt.add<std::string>("_id","_design/vcapt");
		views_pt.add<std::string>("language","javascript");
		views_pt.add<std::string>("views.cameras.map","function(doc) {if(doc.device_tags){var s=doc.device_tags;if((s.indexOf('local_camera')!=-1)||(s.indexOf('network_camera')!=-1)) emit(null, doc);}}");
		views_pt.add<std::string>("views.network_cameras.map","function(doc) {if(doc.device_tags){var s=doc.device_tags;if(s.indexOf('network_camera')!=-1) emit(null, doc);}}");
		views_pt.add<std::string>("views.local_cameras.map","function(doc) {if(doc.device_tags){var s=doc.device_tags;if(s.indexOf('local_camera')!=-1) emit(null, doc);}}");



 		if (!cdb_manager.save_ptree_document(views_pt,error_message))
 			throw std::runtime_error("cannot save _design/vcapt:"+error_message);

		std::cout<<"ok"<<std::endl;
		
		
		//get list of cameras
		boost::property_tree::ptree cameras_ptree;
		if (!cdb_manager.load_ptree_document("_design/vcapt/_view/cameras",cameras_ptree,error_message))
			throw std::runtime_error("_design/vcapt/_view/cameras error:"+error_message+" (db no initialized?)");

		boost::optional<int> cameras_count = cameras_ptree.get_optional<int>("total_rows");
		if (!cameras_count)
			throw std::runtime_error("couch-db incorrect answer: total_rows missed");
		if (cameras_count)
			if ((*cameras_count)>0)
				BOOST_FOREACH(boost::property_tree::ptree::value_type &v,cameras_ptree.get_child("rows")){
					boost::property_tree::ptree pt1 = v.second;
					boost::property_tree::ptree cam_ptree = pt1.get_child("value",boost::property_tree::ptree());

					boost::optional<std::string> doc_id = cam_ptree.get_optional<std::string>("_id");
					if (!doc_id)
						continue;

					std::string emess;
					couchdb::document_ptr main_doc = cdb_manager.get_document(*doc_id,emess);
					if (!main_doc)
						continue;

					//TODO: runtime doc cleanup


// 					Document1Ptr main_doc,temp_doc;
// 					std::string emess;
// 					if (!Camera::check_and_create_camera_documents(&documents_manager,"",(*doc_id),main_doc,temp_doc,emess)){
// 						journal->Write(ERR,DST_SYSLOG|DST_STDERR,"camera doc(%s) init error:%s",(*doc_id).c_str(),emess.c_str());
// 						continue;
// 					}


					//start network-cam
					
					if (main_doc->have_tag("network_camera")){
						try{
 							camera_container_ptr cptr(new camera_container(&cdb_manager,main_doc, boost::bind(&manager::camera_container_stop_handler,this,_1)));
 							cameras.push_back(cptr);
						}
						catch(std::runtime_error& ex){
							using namespace Utility;
							Journal::Instance()->Write(ERR,DST_STDERR|DST_SYSLOG,"cannot start network cam (id:%s): %s",
								main_doc->id().c_str(),
								ex.what());

						}
					}
					

			}
			
}


/*
codec_container_ptr manager::get_codec_container(camera* cam, client_parameters& e_params, tcp_client_ptr c){

	std::cout<<"manager::get_codec_container()"<<std::endl;

	//åñòü òàêîé stream? (cont/codec/bitrate)



	std::deque<codec_container_ptr> this_cam_user_streams;
	std::deque<codec_container_ptr> this_cam_priority_streams;

	camera_container_ptr this_cam_container;

	BOOST_FOREACH(camera_container_ptr cptr,cameras){
		if (cptr->get_camera() != cam)
			continue;

		this_cam_container = cptr;

		BOOST_FOREACH(codec_container_ptr s,cptr->codec_processors){

			if (s->have_clients()){
				if (s->have_priority_clients())
					this_cam_priority_streams.push_back(s); else
					this_cam_user_streams.push_back(s);
			}


			if ((s->get_params().codec_name != e_params.codec_name)||(s->get_params().container_name != e_params.container_name))
				continue ;

			if (e_params.f_size){
				if (s->get_params().codec_format.fsize != e_params.f_size)
					continue;
			}



			if (e_params.bitrate != 0)
				if (s->get_params().codec_format.bitrate != e_params.bitrate)
					continue ;

			return s;
		}	
	}

	if (!this_cam_container)
		throw std::runtime_error("cannot find specified camera");


	bool have_priority_streams = this_cam_priority_streams.size() != 0;
	bool have_user_streams = this_cam_user_streams.size() != 0;

	//----------- need create new stream -----------

	//
	boost::shared_ptr<ffmpeg_encoder> eptr(new ffmpeg_encoder(e_params.container_name,e_params.codec_name));



	stream::parameters s_params;
	s_params.codec_name = e_params.codec_name;
	s_params.container_name = e_params.container_name;



	//check pixel format
	std::deque<AVPixelFormat> encoder_formats = eptr->get_pixel_formats();
	capturer::format cam_format = cam->get_current_format();
	s_params.codec_format.pixfmt= cam_format.ffmpeg_pixfmt;
	if (std::find(encoder_formats.begin(),encoder_formats.end(),s_params.codec_format.pixfmt)==encoder_formats.end()){
		//TODO: if transcoding not enabled ? throw!

		std::cout<<"encoder "<<e_params.codec_name<<" not supported camera pixfmt("<<libav::av_get_pix_fmt_name(cam_format.ffmpeg_pixfmt)<<"). transcoding needed"<<std::endl;
		s_params.codec_format.pixfmt = encoder_formats[0];
	}

	//check bitrate
	s_params.codec_format.bitrate = e_params.bitrate;
	if (s_params.codec_format.bitrate==0)
		s_params.codec_format.bitrate = 500000;




	bool need_change_camera_framesize = false;
	s_params.codec_format.fsize= e_params.f_size;

	if (s_params.codec_format.fsize){

		if (have_priority_streams){
			//we cannot change camera framesize (because have another priority streams)
			need_change_camera_framesize = false;

			//compare e_params.f_size with current fsize
			if (s_params.codec_format.fsize > cam->get_current_framesize()){

				if (c->internal_client){
					need_change_camera_framesize = true;
				} else
					throw std::runtime_error("requested framesize > framesize of existing prio-stream");

			}


		} else
			if (have_user_streams){

				if (c->internal_client){
					//we are priority stream, change cam->fsize if neñessary
					need_change_camera_framesize = cam->get_current_framesize() != s_params.codec_format.fsize;
				} else{
					//change cam->fsize only is e_params.f_size > cam->get_framesize()
					need_change_camera_framesize = s_params.codec_format.fsize > cam->get_current_framesize();
				}
			} else{
				//no have another streams
				need_change_camera_framesize = cam->get_current_framesize() != s_params.codec_format.fsize;
			}

	} else{
		//framesize not defined. get framesize of cam
		if (have_priority_streams || have_user_streams){
			need_change_camera_framesize = false;
			s_params.codec_format.fsize = cam->get_current_framesize();

		} else{
			s_params.codec_format.fsize = frame_size(640,480);
			need_change_camera_framesize = s_params.codec_format.fsize != cam->get_current_framesize();
		}
	}


	if (need_change_camera_framesize){
		std::cout<<"need change camera framesize (to "<<s_params.codec_format.fsize.to_string()<<")"<<std::endl;
		//check e_params.f_size for camera capabilities
		if (!cam_format.check_framesize(s_params.codec_format.fsize))
			throw std::runtime_error("requested framesize not supported by camera");


		//try to change camera framesize
		cam->set_framesize(s_params.codec_format.fsize);
	}


	codec_container_ptr sptr(new stream(this, cam, eptr,s_params));
	this_cam_container->codec_processors.push_back(sptr);
	std::cout<<"streams.size()="<<this_cam_container->codec_processors.size()<<std::endl;


	return sptr;


}
*/

void manager::do_videodev_appearance(const std::string& devname){
	//std::cout<<"appearance:"<<devname<<std::endl;
	try{
		//TODO: try to read unique camera identifier
#if defined(LinuxPlatform)
		capturer::definition def;
		if (!camera_v4l2::read_v4l2_device_definition(devname,-1,def))
			throw std::runtime_error("cannot read device definition of "+devname);
		std::string s1 = def.bus_info + def.device_name + def.manufacturer_name + def.slot_name;
		std::string unique_id =  Utility::Sha::calc((unsigned char*)s1.c_str(),s1.size());
#else
		std::string unique_id = "windows_cam";
#endif

		//find/create document
		std::string error_message;
		main_doc = find_local_camera_document(unique_id);
		if (!main_doc){
			main_doc = cdb_manager.create_document(error_message);
			main_doc->add_tag(cprops::LocalCameraTag);
			main_doc->set_property<std::string>(cprops::UniqueId,unique_id);
		}
		main_doc->set_property<std::string>(cprops::Url,devname);


		//create container

		camera_container_ptr cptr(new camera_container(&cdb_manager,main_doc,boost::bind(&manager::camera_container_stop_handler,this,_1)));
		cameras.push_back(cptr);		
	}
	catch(std::runtime_error& ex){
		using namespace Utility;
		Journal::Instance()->Write(ERR,DST_SYSLOG|DST_STDERR,"cannot dispatch videodev appearance:%s",ex.what());
	}
}
void manager::do_videodev_disappearance(const std::string& devname){
	std::cout<<"disappearance:"<<devname<<std::endl;

}


void manager::camera_container_stop_handler(camera_container*){
	std::cout<<"camera_container_stop_handler"<<std::endl;
	//TODO
}




void manager::dispatch_new_client(tcp_client_ptr client){
	try{

		if (client->url_keypairs.find("id")==client->url_keypairs.end())
			throw std::runtime_error("HTTP/1.0 400 bad request:camera id not defined");
		std::string camera_id = client->url_keypairs["id"];
		//check camera id
		camera_container_ptr cptr;
		BOOST_FOREACH(camera_container_ptr c,cameras)
			if (c->camera_id() == camera_id){
				cptr = c;
				break;
			}
		if (!cptr)
			throw std::runtime_error("HTTP/1.0 400 bad request:camera not found");


		cptr->add_network_client(client);

	}
	catch(std::runtime_error& ex){
		using namespace Utility;
		Utility::Journal::Instance()->Write(NOTICE,DST_STDERR|DST_SYSLOG,"http-client discarder:%s",ex.what());
		//construct http-error answer
		std::ostringstream oss;
		oss <<ex.what()<<std::endl;
		boost::system::error_code ec;
		client->get_socket().send(boost::asio::buffer(oss.str()),0,ec);	
	}
}


void manager::start_acceptor(boost::asio::ip::tcp::acceptor& a, unsigned short port){
	using namespace boost::asio::ip;

	//creating tcp-ports
	tcp::endpoint ep(tcp::endpoint(tcp::v4(),port));
	a.open(ep.protocol());
	boost::system::error_code ec;

	int fail_count = 0;
	do{
		a.bind(ep,ec);
		if (ec){
			std::cerr<<"bind error (port "<<port<<" busy...?)"<<std::endl;
			boost::this_thread::sleep(boost::posix_time::seconds(1));
			fail_count++;
		}

	}
	while((ec)&&(fail_count<3));

	if (fail_count<3){
		a.listen();
		std::cout<<"tcp-port "<<int(port)<<" binded and switched to LISTEN-state"<<std::endl;

		tcp_client_ptr new_client(new tcp_client(internal_ioservice)); 
		a.async_accept(new_client->get_socket(),
			boost::bind(&manager::handle_tcp_accept, this, port, new_client,boost::asio::placeholders::error));
	} else{
		throw std::runtime_error("cannot create TCP port for streaming");
	}

}

void manager::handle_tcp_accept(unsigned short port, tcp_client_ptr cptr,boost::system::error_code ec){
	if (ec)
		return ;
	std::cout<<"port("<<int(port)<<"): connection accepted"<<std::endl;

	cptr->internal_client = port == SystemTcpPortNum ? true : false;

	//async receive http-request
	memset(cptr->recv_buffer,0,CLIENT_BUF_SIZE);
	cptr->received_bytes = 0;
	cptr->_socket.async_read_some(
		boost::asio::buffer(cptr->recv_buffer, CLIENT_BUF_SIZE),
		boost::bind(&manager::handle_receive_bytes, 
		this,
		cptr,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));


	//starting new accept
	tcp_client_ptr new_client(new tcp_client(internal_ioservice));
	new_client->port = port;
	switch(port){
		case UserTcpPortNum:
			u_acceptor.async_accept(new_client->get_socket(),
				boost::bind(&manager::handle_tcp_accept, this, port, new_client,boost::asio::placeholders::error));
			break;
		case SystemTcpPortNum:
			s_acceptor.async_accept(new_client->get_socket(),
				boost::bind(&manager::handle_tcp_accept, this, port, new_client,boost::asio::placeholders::error));
			break;
	}

}


std::deque<std::string> __parse_string_by_separator__(const std::string& src, const std::string& separator);

void manager::handle_receive_bytes(tcp_client_ptr client,boost::system::error_code ec,size_t bytes_transferred){
	if (ec)
		return ;
	
	if (bytes_transferred==0)
		return ;


	printf("%s",client->recv_buffer+client->received_bytes);

	client->received_bytes += bytes_transferred;

	//find empty string
	std::string body(client->recv_buffer);
	char c1[] = {0x0D,0x0A,0x0D,0x0A};
	std::string s1;
	s1 += char(0x0d);
	s1 += char(0x0a);
	s1 += char(0x0d);
	s1 += char(0x0a);
	//std::string s1(c1);
	std::string::size_type pp = body.find(s1);
	if (pp != std::string::npos){
		//All headers received. time to parse
		std::string sep;
		sep += char(0x0d);
		sep += char(0x0a);
		client->http_request_headers = __parse_string_by_separator__(body,sep);
		if (client->http_request_headers.size()){
			std::string str1 = client->http_request_headers[0];
			std::deque<std::string> str1_parts = __parse_string_by_separator__(str1," ");
			if (str1_parts.size()==3){
				client->http_method = str1_parts[0];
				client->http_url = str1_parts[1];
				client->http_proto = str1_parts[2];

				//parse client->http_url by '?'
				std::deque<std::string> parts = __parse_string_by_separator__(client->http_url,"?");
				std::string p;
				BOOST_FOREACH(p,parts){
					std::deque<std::string> kv_deque = __parse_string_by_separator__(p,"=");
					if (kv_deque.size()==2){
						client->url_keypairs[kv_deque[0]] = kv_deque[1];
					}
				}


				//transact http-client to Manager
				dispatch_new_client(client);

			}
		}

		return ;
	}


	//TODO: defence from long-time receive (mb Timer ??)


	if (client->received_bytes < CLIENT_BUF_SIZE){
		//async receive http-request
		client->_socket.async_read_some(
			boost::asio::buffer(client->recv_buffer + client->received_bytes, CLIENT_BUF_SIZE - client->received_bytes),
			boost::bind(&manager::handle_receive_bytes, 
			this,
			client,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));

	}

}

std::deque<std::string> __parse_string_by_separator__(const std::string& src, const std::string& separator){
	std::string::size_type p = 0;
	std::deque<std::string> ret_value;
	while(p < src.size()){
		std::string::size_type next_p = src.find(separator,p);
		if (next_p==std::string::npos)
			break;
		std::string _part;
		if (next_p - p > 0)
			_part = src.substr(p,next_p - p);		

		ret_value.push_back(_part);



		p = next_p + separator.size();
	}

	if (p < src.size()){
		std::string last_part = src.substr(p,src.size() - p);
		ret_value.push_back(last_part);
	}

	return ret_value;
}


void manager::do_local_devices_scan(boost::system::error_code ec){

	if (ec)
		return ;

	if (!internal_ioservice_work_ptr)
		return ;

	typedef std::map<std::string,int> M1;
	BOOST_FOREACH(M1::value_type& vt,videodevices){
		int _state = 0;
		boost::system::error_code ec;
		if (boost::filesystem::exists(vt.first,ec))
			_state++;

		if (_state != vt.second){
			vt.second = _state;
			if (_state>0)
				do_videodev_appearance(vt.first); else
				do_videodev_disappearance(vt.first);
		}
	}
	local_devices_scan_timer.expires_from_now(boost::chrono::milliseconds(1000));
	local_devices_scan_timer.async_wait(boost::bind(&manager::do_local_devices_scan,this,boost::asio::placeholders::error));
}


// manager::camera_container::camera_container():_opencv_camera(0),_v4l2_camera(0){
// 
// }



void manager::finalize_codec_processor(camera_container* _container, codec_processor* _processor){
	internal_ioservice.post(boost::bind(&manager::i_thread_finalize_codec_processor,this,_container,_processor));
}

void manager::i_thread_finalize_codec_processor(camera_container* _container, codec_processor* _processor){
	BOOST_FOREACH(camera_container_ptr cptr,cameras)
		if (cptr.get()==_container){
			cptr->delete_codec_processor(_processor);
			return ;
		}
}



void manager::internal_thread_proc(){

	try{
		internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>(
			new boost::asio::io_service::work(internal_ioservice));
		internal_ioservice.run();
	}
	catch(...){

	}


}

couchdb::document_ptr manager::find_local_camera_document(const std::string& camera_unique_id){
	//get list of local-cameras
	std::string error_message;
	boost::property_tree::ptree cameras_ptree;

	if (!cdb_manager.load_ptree_document("_design/vcapt/_view/local_cameras",cameras_ptree,error_message))
		throw std::runtime_error("_design/vcapt/_view/local_cameras error:"+error_message+" (db no initialized?)");

	boost::optional<int> cameras_count = cameras_ptree.get_optional<int>("total_rows");
	if (!cameras_count)
		throw std::runtime_error("couch-db incorrect answer: total_rows missed");
	if (cameras_count)
		if ((*cameras_count)>0)
			BOOST_FOREACH(boost::property_tree::ptree::value_type &v,cameras_ptree.get_child("rows")){
				boost::property_tree::ptree pt1 = v.second;
				boost::property_tree::ptree cam_ptree = pt1.get_child("value",boost::property_tree::ptree());

				std::string cam_unique_id = cam_ptree.get(cprops::UniqueId,"");
				if (cam_unique_id == camera_unique_id){
					std::string doc_id = cam_ptree.get("_id","");
					return cdb_manager.get_document(doc_id,error_message);
				}
		}
	return couchdb::document_ptr();
}




