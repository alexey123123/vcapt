#include <boost/foreach.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/chrono.hpp>

#include "couchdb.h"
#include "camera_container.h"
#include "camera_opencv.h"
#include "camera_v4l2.h"
#include "camera_openmax.h"
#include "filter.h"
#include "types.h"
#include "draw.h"


#define FSTREAM_LEN_CONTROL_IVAL_SECS 5
#define FRAME_GENERATOR_CLEANUP_TIMEOUT 10


camera_container::camera_container(couchdb::manager* _cdb_manager, archive_client* _a_client, couchdb::document_ptr d, stop_handler sh):
					cdb_manager(_cdb_manager),a_client(_a_client),main_doc(d),_stop_handler(sh),
					filestream_length_control_timer(internal_ioservice),
					retry_filestream_start_timer(internal_ioservice),
					frame_generator_cleanup_timer(internal_ioservice){

	std::string error_message;

	//create runtime document
	std::string runtime_doc_id = main_doc->get_property<std::string>(cprops::runtime_doc_id,"");
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
			main_doc->set_property<std::string>(cprops::runtime_doc_id, runtime_doc->id());
	}
	runtime_doc->add_tag("temp");


	std::string d_id = main_doc->get_property<std::string>(cprops::streams_doc_id,"");
	if (d_id != "")
		streams_doc = cdb_manager->get_document(d_id,error_message);
	if (!streams_doc){
		streams_doc = cdb_manager->create_document(error_message);
		if (!streams_doc)
			throw std::runtime_error("cannot create streams_doc");

		main_doc->set_property<std::string>(cprops::streams_doc_id,streams_doc->id());
	}
	//defaults
	streams_doc->set_property<int>(cprops::streams_save_video,0,true);
	streams_doc->set_property<int>(cprops::streams_video_max_size,500,true);
	streams_doc->set_property<int>(cprops::streams_video_max_duration,60,true);
	streams_doc->set_property<std::string>(cprops::streams_video_format,"avi",true);
	streams_doc->set_property<int>(cprops::streams_video_bitrate,1000000,true);
	streams_doc->set_property<std::string>(cprops::streams_video_framesize,"",true);

	streams_doc_tptr = streams_doc->set_handlers(
		boost::bind(&camera_container::doc_changed,this,_1,_2,_3),
		boost::bind(&camera_container::doc_deleted,this));


	d_id = main_doc->get_property<std::string>(cprops::controls_doc_id,"");
	if (d_id != "")
		controls_doc = cdb_manager->get_document(d_id,error_message);
	if (!controls_doc){
		controls_doc = cdb_manager->create_document(error_message);
		if (!controls_doc)
			throw std::runtime_error("cannot create controls_doc");

		main_doc->set_property<std::string>(cprops::controls_doc_id,controls_doc->id());
	}
	//defaults
	controls_doc->set_property<int>(cprops::controls_DisplayCameraName,0,true);
	controls_doc->set_property<std::string>(cprops::controls_CameraName,"",true);
	controls_doc->set_property<int>(cprops::controls_RotateAngle,0,true);
	controls_doc->set_property<int>(cprops::controls_WriteDateTimeText,0,true);

	controls_doc_tptr = controls_doc->set_handlers(
		boost::bind(&camera_container::doc_changed,this,_1,_2,_3),
		boost::bind(&camera_container::doc_deleted,this));

	internal_ioservice.post(boost::bind(&camera_container::i_thread_controls_doc_change,this,"",""));

	



	_camera = 0;


	capturer::connect_parameters cp;
	cp.maximum_buffer_size_mb = 10;
	//TODO: get connect parameters from document
	cp.connection_string = main_doc->get_property<std::string>(cprops::url,"");

	if ((cp.connection_string==std::string(WindowsCameraDevname))||(main_doc->have_tag(cprops::network_camera_tag))){
		//it is a local windows cam or http
		cp.connect_attempts_interval = 1000;
		cp.max_connect_attempts = 0xFFFFFFF;
		_camera = new camera_opencv(cp,
			boost::bind(&camera_container::camera_state_change_handler,this,_1,_2), 
			boost::bind(&camera_container::stop_camera_handler,this,_1));
	} else{
		if (main_doc->have_tag(cprops::local_camera_tag)){
			//v4l2
			cp.connect_attempts_interval = 0;
			cp.max_connect_attempts = 1;
			_camera = new camera_v4l2(cp,
				boost::bind(&camera_container::camera_state_change_handler,this,_1,_2), 
				boost::bind(&camera_container::stop_camera_handler,this,_1));
		}
		if (main_doc->have_tag(cprops::rpi_camera_tag)){
			//RPI
			cp.connect_attempts_interval = 0;
			cp.max_connect_attempts = 1;
			_camera = new camera_openmax(cp,
				boost::bind(&camera_container::camera_state_change_handler,this,_1,_2), 
				boost::bind(&camera_container::stop_camera_handler,this,_1));
		}

	}
	
	main_doc->set_property<int>(cprops::camera_state,_camera->get_state());

	main_doc_tptr = main_doc->set_handlers(
		boost::bind(&camera_container::doc_changed,this,_1,_2,_3),
		boost::bind(&camera_container::doc_deleted,this));

	internal_ioservice.post(boost::bind(&camera_container::i_thread_dispatch_streams_doc_change,this,
		boost::system::error_code()));


	filestream_length_control_timer.expires_from_now(boost::chrono::seconds(FSTREAM_LEN_CONTROL_IVAL_SECS));
	filestream_length_control_timer.async_wait(boost::bind(&camera_container::i_thread_do_filestream_length_control,
		this,boost::asio::placeholders::error));

	//we need to periodically clean-up generated error-frames (memory usage optimization)
	frame_generator_cleanup_timer.expires_from_now(boost::chrono::seconds(5));
	frame_generator_cleanup_timer.async_wait(boost::bind(&camera_container::i_thread_do_frame_generator_cleanup,
		this,boost::asio::placeholders::error));




	internal_thread = boost::thread(boost::bind(&camera_container::internal_thread_proc,this));
}

camera_container::~camera_container(){
	boost::system::error_code ec;

	internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>();	


	filestream_length_control_timer.cancel(ec);
	retry_filestream_start_timer.cancel(ec);
	frame_generator_cleanup_timer.cancel(ec);

	main_doc_tptr = couchdb::document::ticket_ptr();
	streams_doc_tptr = couchdb::document::ticket_ptr();
	controls_doc_tptr = couchdb::document::ticket_ptr();;

	codec_processors.clear();
	last_frame = frame_ptr();
	delete _camera;
	internal_thread.join();
}


void camera_container::add_network_client(tcp_client_ptr c){
	client_parameters e_params;
	std::string error_message;
	if (!e_params.construct(c,error_message))
		throw std::runtime_error("HTTP/1.0 400 bad request:"+error_message);
	e_params.codec_id = codec_processor::select_codec_for_container(e_params.container_name);
	if (e_params.codec_id==AV_CODEC_ID_NONE)
		throw std::runtime_error("cannot find codec for container "+e_params.container_name);
	if (!e_params.f_size)
		e_params.f_size = _camera->get_current_framesize();

	//DEBUG
// 	if (!c->internal_client){
// 		e_params.f_size = boost::optional<frame_size>();
// 		e_params.bitrate = boost::optional<int>();
// 	}
		

	codec_processor_ptr cptr;
	//1. try to find similar processor (equal codec/container/framesize/bitrate)
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
		c_f.codec_id = e_params.codec_id;

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

	codec_processor_ptr proc_ptr(new codec_processor(this,new_codec,
		boost::bind(&camera_container::delete_codec_processor,this,_1)));

	proc_ptr->try_to_add_network_client(e_params,c);
	

	try{
		boost::unique_lock<boost::mutex> l1(internal_mutex);

		_camera->change_streaming_state(camera::ss_started);

		codec_processors.push_back(proc_ptr);

	}
	catch(std::runtime_error& ex){
		using namespace Utility;
		Journal::Instance()->Write(CRIT,DST_STDERR|DST_SYSLOG,"Cannot start streaming: %s",ex.what());
	}


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

	if (codec_processors.size()==0)
		_camera->change_streaming_state(camera::ss_stopped);

}


void camera_container::internal_thread_proc(){
	try{
		internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>(
			new boost::asio::io_service::work(internal_ioservice));
		internal_ioservice.run();
	}
	catch(...){
		using namespace Utility;
		Journal::Instance()->Write(ALERT,DST_STDERR|DST_SYSLOG,"unhandled exception in camera_container::internal_thread");
	}
}

void camera_container::i_thread_dispatch_url_change(std::string new_url){
	if (main_doc->have_tag(cprops::network_camera_tag)){
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

void camera_container::doc_changed(std::string doc_id,std::string property_name,std::string property_value){
	using namespace Utility;
	std::cout<<"doc changed: "<<property_name<<"="<<property_value<<std::endl;
	//---------------- streams doc changes ---------------
	if (streams_doc)
		if (doc_id == streams_doc->id()){
			internal_ioservice.post(boost::bind(&camera_container::i_thread_dispatch_streams_doc_change,this,
				boost::system::error_code()));
		}


	//---------------- main doc changes ---------------
	if (main_doc)
		if (doc_id == main_doc->id()){
			if (property_name==cprops::url)
				internal_ioservice.post(boost::bind(&camera_container::i_thread_dispatch_url_change,this,property_value));

			if (property_name==cprops::frame_size){
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


				internal_ioservice.post(boost::bind(&camera_container::i_thread_dispatch_framesize_change,this,fs));
			}

		}
	if (controls_doc)
		internal_ioservice.post(boost::bind(&camera_container::i_thread_controls_doc_change,this,"",""));
}
void camera_container::doc_deleted(){

}

void camera_container::i_thread_dispatch_framesize_change(frame_size fs){
	boost::unique_lock<boost::mutex> l1(last_frame_mutex);
	last_frame = frame_ptr();
	error_frames.clear();

	_camera->change_streaming_state(camera::ss_stopped);


	_camera->set_framesize(fs);
	printf("changed!\n");
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
		runtime_doc->set_property<int>(cprops::camera_state, new_state);
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
			main_doc->set_property<std::string>(cprops::possible_framesizes,
				__pack_framesizes_to_string__(poss_fsizes));

			frame_size db_framesize;
			db_framesize.from_string(main_doc->get_property<std::string>(cprops::frame_size,""),frame_size(640,480));

			//setup
			frame_size c_fs = _camera->get_current_framesize();
			if (c_fs != db_framesize)
				_camera->set_framesize(db_framesize);

			//update database
			main_doc->set_property<std::string>(cprops::frame_size,db_framesize.to_string());
		}
		catch(std::runtime_error& ex){
			using namespace Utility;
			Journal::Instance()->Write(ERR,DST_STDERR|DST_SYSLOG,"camera:%s: cannot sync framesize:%s",
				main_doc->id().c_str(),
				ex.what());
		}

		//TODO: sync controls






	}

}



void camera_container::i_thread_dispatch_streams_doc_change(boost::system::error_code ec){
	if (ec)
		return ;
	if (!streams_doc)
		return ;

	try{
		filestream_params fp;
		fp.from_doc(streams_doc);



		bool enabled = streams_doc->get_property<int>(cprops::streams_save_video,false) == 1;

		// need finalize file stream?
		bool need_finalize = false;
		need_finalize |= (!enabled);
		need_finalize |= fp.c_params.bitrate != f_params.c_params.bitrate;
		need_finalize |= fp.c_params.container_name != f_params.c_params.container_name;
		need_finalize |= fp.c_params.f_size != f_params.c_params.f_size;


		if (need_finalize){
			if (next_file_stream){
				stream_ptr sp = next_file_stream;
				next_file_stream = stream_ptr();
				BOOST_FOREACH(codec_processor_ptr cptr,codec_processors)
					cptr->delete_stream(sp.get());
				sp = stream_ptr();
			}
			if (current_file_stream){
				stream_ptr sp = current_file_stream;
				current_file_stream = stream_ptr();
				BOOST_FOREACH(codec_processor_ptr cptr,codec_processors)
					cptr->delete_stream(sp.get());
				sp = stream_ptr();
			}

			request_to_next_delivered = false;
		}

		f_params = fp;

		if (!enabled)
			return ;

		/**/
		if (!current_file_stream){
			//get assumed filesize

			//call archive client for filepath
			a_client->create_long_writing_file(archive_client::ft_video,
				f_params.get_assumed_filesize(),
				_camera->get_definition().device_name,
				boost::bind(&camera_container::dispatch_answer_from_archive_server,this,_1,_2,f_params));
		}

	}
	catch(std::runtime_error& ex){
		using namespace Utility;
		Journal::Instance()->Write(NOTICE,DST_SYSLOG|DST_STDOUT,"stream-doc dispatch error:%s",ex.what());

		//TODO: ????
		return ;

	}





	
}

void camera_container::dispatch_answer_from_archive_server(boost::system::error_code ec,
	long_writing_file_ptr fptr,
	const filestream_params fp){

		if (ec){
			using namespace Utility;
			Journal::Instance()->Write(NOTICE,DST_SYSLOG|DST_STDOUT,"get an error from archive-server");
			request_to_next_delivered = false;
			//try again over 5 sec

			retry_filestream_start_timer.expires_from_now(boost::chrono::seconds(5));
			retry_filestream_start_timer.async_wait(boost::bind(&camera_container::i_thread_dispatch_streams_doc_change,this,
				boost::asio::placeholders::error));

			return ;
		}			



		internal_ioservice.post(boost::bind(&camera_container::i_thread_create_filestream,this,fptr,fp));

}


void camera_container::i_thread_create_filestream(long_writing_file_ptr fptr,
	const filestream_params fp)
{

	request_to_next_delivered = false;

	try{
	

		//1. detect codec
		AVCodecID c_id = codec_processor::select_codec_for_container(fp.c_params.container_name);
		if (c_id==AV_CODEC_ID_NONE)
			throw std::runtime_error("cannot find codec for container "+fp.c_params.container_name);


		//TODO: !!!!!
		//if (c_id==AV_CODEC_ID_H264)
		//	c_id = AV_CODEC_ID_MSMPEG4V2;

		frame_size camera_framesize = _camera->get_current_framesize();

		codec_processor_ptr needed_cptr;
		BOOST_FOREACH(codec_processor_ptr cptr,codec_processors){
			if (cptr->get_codec()->get_avcodec_context()->codec_id != c_id)
				continue;
			
			if (fp.c_params.bitrate!=0)
				if (cptr->get_codec()->get_format().bitrate != fp.c_params.bitrate)
					continue;

			//skip codecs with different framesize
			if (cptr->get_codec()->get_format().fsize != camera_framesize)
				continue;

			//got it!
			needed_cptr = cptr;
			break;
		}
		if (!needed_cptr){
			//need create new codec processor
			codec::format f;
			f.input_pixfmt = _camera->get_current_format().ffmpeg_pixfmt;
			f.codec_id = c_id;

			f.bitrate = *fp.c_params.bitrate;
			f.fsize = camera_framesize;
			
			/*
			std::string fsize_str = main_doc->get_property<std::string>(cprops::frame_size,"");
			f.fsize.from_string(fsize_str,frame_size(640,480));
			if (fp.c_params.f_size)
				f.fsize = *fp.c_params.f_size;

			if (!_camera->get_current_framesize())
				throw std::runtime_error("camera not ready");


			if (f.fsize > _camera->get_current_framesize()){
				using namespace Utility;
				f.fsize = _camera->get_current_framesize();
				Journal::Instance()->Write(INFO,DST_STDOUT|DST_SYSLOG,"Filestream: framesize decreased(to %s) due to camera settings",
					f.fsize.to_string().c_str());
				
			}
			*/


				
			
			codec_ptr cp = codec::create(f);
			if (!cp)
				throw std::runtime_error("cannot create codec");

			needed_cptr = codec_processor_ptr(
				new codec_processor(this,cp,
				boost::bind(&camera_container::delete_codec_processor,this,_1)));

			codec_processors.push_back(needed_cptr);
		}
		if (!needed_cptr)
			throw std::runtime_error("cannot create codec processor");

		boost::filesystem::path file_path = fptr->get_path();

		//filename
		std::ostringstream fname_oss;
		fname_oss<<"video_";	
		boost::posix_time::ptime lt = boost::posix_time::second_clock::local_time();
		tm _t = boost::posix_time::to_tm(lt);
		fname_oss<<_t.tm_mday<<"_"<<_t.tm_mon<<"_"<<_t.tm_year+1900<<"_"<<
			_t.tm_hour<<"_"<<_t.tm_min<<"_"<<_t.tm_sec;
		fname_oss<<"."<<f_params.c_params.container_name;

		boost::filesystem::path full_path = file_path / fname_oss.str();

		stream_ptr sptr = av_container_stream::create_file_stream(fp,
			needed_cptr->get_codec()->get_avcodec_context(),
			boost::bind(&camera_container::filestream_stop_handler,this,_1),
			boost::bind(&camera_container::filestream_progress_handler,this,_1,_2,_3,_4),
			full_path);	

		needed_cptr->add_stream(sptr);


		if (!current_file_stream){
			current_file_stream = sptr;
			current_file_stream->start();
		} else
			if (!next_file_stream)
				next_file_stream = sptr;

	}
	catch(std::runtime_error& ex){
		using namespace Utility;
		Journal::Instance()->Write(NOTICE,DST_SYSLOG|DST_STDOUT,"filestream creation error:%s",ex.what());
		
		retry_filestream_start_timer.expires_from_now(boost::chrono::seconds(5));
		retry_filestream_start_timer.async_wait(boost::bind(&camera_container::i_thread_dispatch_streams_doc_change,this,
			boost::asio::placeholders::error));

	}
}

void camera_container::filestream_stop_handler(stream* s){
	internal_ioservice.post(boost::bind(&camera_container::i_thread_stop_filestream,this,s));
}

void camera_container::i_thread_stop_filestream(stream* s){
	printf("--camera_container::i_thread_stop_filestream\n");
	bool destruction_state = internal_ioservice_work_ptr.get() == 0;
	
	if (current_file_stream)
		if (current_file_stream.get() == s){
			
			//TODO: notify archive server that file is completed
			
			//stopping current stream
			current_file_stream = stream_ptr();

			//starting next stream
			if (!destruction_state){
				if (next_file_stream){
					current_file_stream = next_file_stream;
					next_file_stream = stream_ptr();
					current_file_stream->start();
					std::cout<<"next filestream started"<<std::endl;
				} else{
					//abnormal finalization of current_file_stream
					//restart streaming
					using namespace Utility;
					Journal::Instance()->Write(NOTICE,DST_SYSLOG|DST_STDOUT,"Cam:%s: unexpected filestream finalization. restart",
						main_doc->id().c_str());
					retry_filestream_start_timer.expires_from_now(boost::chrono::seconds(1));
					retry_filestream_start_timer.async_wait(boost::bind(&camera_container::i_thread_dispatch_streams_doc_change,this,
						boost::asio::placeholders::error));

				}
			}
		}
			
		if (!destruction_state){
			BOOST_FOREACH(codec_processor_ptr cptr,codec_processors)
				cptr->delete_stream(s);
		}



}

void camera_container::filestream_progress_handler(stream*,
	boost::system::error_code,
	boost::chrono::steady_clock::duration,	//current duration
	boost::int64_t){

}

class ex_formirate_next_stream{};
void camera_container::i_thread_do_filestream_length_control(boost::system::error_code ec){
	if (ec)
		return ;

	try{
		if (current_file_stream){
			//check stream duration
			int current_dur_secs = current_file_stream->get_duration_secs();
			if (current_dur_secs > 0){
				if (streams_doc){
					int max_dur_mins = streams_doc->get_property<int>(cprops::streams_video_max_duration,-1);
					if (max_dur_mins > 0){					
						if (current_dur_secs + FSTREAM_LEN_CONTROL_IVAL_SECS*2 > max_dur_mins*60){
							//time to formirate next stream
							throw ex_formirate_next_stream();
						}
					}
				}

				//check stream size
				boost::int64_t max_wbytes = streams_doc->get_property<boost::int64_t>(cprops::streams_video_max_size,-1);
				if (max_wbytes > 0){
					boost::int64_t wbytes = current_file_stream->get_written_bytes();
					boost::int64_t stream_write_speed = wbytes / current_dur_secs;
					if (wbytes + stream_write_speed * (FSTREAM_LEN_CONTROL_IVAL_SECS*2) > max_wbytes * 1024 * 1024){
						//time to formirate next stream
						throw ex_formirate_next_stream();
					}

				}

			}
			
		}
	}
	catch(ex_formirate_next_stream){
		if (!next_file_stream){
			if (!request_to_next_delivered){
				std::cout<<"-- Time to construct next stream! --"<<std::endl;
				filestream_params fp;
				fp.from_doc(streams_doc);

				//call archive client for filepath
				a_client->create_long_writing_file(archive_client::ft_video,
					fp.get_assumed_filesize(),
					_camera->get_definition().device_name,
					boost::bind(&camera_container::dispatch_answer_from_archive_server,this,_1,_2,f_params));


				request_to_next_delivered = true;
			}
			
		}
		
	}
	catch(std::runtime_error& ex){
		//TODO: ???
	}
	filestream_length_control_timer.expires_from_now(boost::chrono::seconds(FSTREAM_LEN_CONTROL_IVAL_SECS));
	filestream_length_control_timer.async_wait(boost::bind(&camera_container::i_thread_do_filestream_length_control,
		this,boost::asio::placeholders::error));
}


std::string get_state_title(capturer::state st);

class __frame_state_exception{
public:
	__frame_state_exception(camera_container::frame_state fs):_state_code(fs){};
	camera_container::frame_state _state_code;
};

class __capturer_state_exception{
public:
	__capturer_state_exception(capturer::state fs):_state_code(fs){};
	capturer::state _state_code;
};

camera_container::frame_state camera_container::get_frame2(
	frame_size fs,
	AVPixelFormat fmt,
	boost::chrono::steady_clock::time_point last_frame_tp, 
	frame_ptr& _fptr){

		frame_state ret_state = fs_Ok;
		frame_size camera_framesize;

		typedef boost::shared_ptr<boost::unique_lock<boost::mutex> > UL_ptr;

		UL_ptr l1;

		try{
			//check camera state
			capturer::state current_state = _camera->get_state();
			if (current_state != capturer::st_Ready)
				throw __capturer_state_exception(current_state);

			capturer::format fmt = _camera->get_current_format();
			//check current framesize
			camera_framesize = _camera->get_current_framesize();
			std::string error_message;
			if (!_frame_helper.check_conversion(camera_framesize,fmt.ffmpeg_pixfmt,fs,fmt.ffmpeg_pixfmt,error_message))
				throw __frame_state_exception(fs_FramesizeOrFormatDiffers);


			_fptr = frame_ptr();

			//fast lock and check last frame
			last_frame_spinlock.lock();
			if (last_frame)
				if (last_frame->tp > last_frame_tp){
					_fptr = last_frame;
				}
			last_frame_spinlock.unlock();
			if (_fptr)
				return fs_Ok;

			//more deep lock
			//boost::unique_lock<boost::mutex> l1(last_frame_mutex);
			l1 = UL_ptr(new boost::unique_lock<boost::mutex>(last_frame_mutex));

			//check camera state again
			current_state = _camera->get_state();
			if (current_state != capturer::st_Ready)
				throw __capturer_state_exception(current_state);

			//check current framesize again
			camera_framesize = _camera->get_current_framesize();
			fmt = _camera->get_current_format();
			if (!_frame_helper.check_conversion(camera_framesize,fmt.ffmpeg_pixfmt,fs,fmt.ffmpeg_pixfmt,error_message))
				throw __frame_state_exception(fs_FramesizeOrFormatDiffers);


			//check last frame again
			if (last_frame)
				if (last_frame->tp > last_frame_tp){
					_fptr = last_frame;
					return fs_Ok;
				}

			//need new frame
			_fptr = _camera->get_frame(last_frame_tp);


		}
		catch(__frame_state_exception& ex){
			//incorrect camera parameters (framesize,etc...)
			ret_state = ex._state_code;
		}
		catch(__capturer_state_exception& ex){
			//need formirate error frame
			if (!l1)
				l1 = UL_ptr(new boost::unique_lock<boost::mutex>(last_frame_mutex));

			last_frame = frame_ptr();

			std::string title = get_state_title(ex._state_code);

			frame_ptr f = _frame_generator.get(fs,title,300,last_frame_tp);
			if (f){
				f->capturer_state = ex._state_code;
				f->rotated = true;
				_fptr = f;
			}
		}

		//_draw.draw_text(_fptr->avframe->data[0],50,50,_fptr->avframe->width,"123",1);


		//apply filters
		if ((ret_state == fs_Ok)&&(_fptr)){
			if ((rotate_filter_string != "")||(main_filter_string != "")){
				if (rotate_filter_string!="")
					if (!_fptr->rotated){
						frame_ptr f1(new frame());
						f1->tp = _fptr->tp;
						f1->capturer_state = _fptr->capturer_state;
						f1->avframe = rotate_filter.apply_to_frame(_fptr->avframe,rotate_filter_string);
						f1->rotated = true;

						_fptr = f1;
					}
					if (main_filter_string!=""){
						frame_ptr f1(new frame());
						f1->tp = _fptr->tp;
						f1->capturer_state = _fptr->capturer_state;
						f1->avframe = main_filter.apply_to_frame(_fptr->avframe,main_filter_string);
						_fptr = f1;

					}
			}
		}


	last_frame_spinlock.lock();
	last_frame = _fptr;
	last_frame_spinlock.unlock();

	return ret_state;
}



void camera_container::get_frame(boost::chrono::steady_clock::time_point last_frame_tp, frame_ptr& _fptr){
	_fptr = frame_ptr();

	//fast lock and check last frame
	last_frame_spinlock.lock();
	if (last_frame)
		if (last_frame->tp > last_frame_tp){
			_fptr = last_frame;
		}
	last_frame_spinlock.unlock();
	if (_fptr)
		return ;

	//more deep lock
	boost::unique_lock<boost::mutex> l1(last_frame_mutex);

	//check last frame again
	if (last_frame)
		if (last_frame->tp > last_frame_tp){
			_fptr = last_frame;
			return ;
		}

	//need new frame
	frame_ptr f;
	capturer::state current_state = _camera->get_state();
	switch(current_state){
		case capturer::st_Ready:
			f = _camera->get_frame(last_frame_tp);
			break;
		default:{
			//use generated frame or render a new one
			
			frame_size current_fsize = _camera->get_current_framesize();
			if (!current_fsize)
				current_fsize = frame_size(640,480);
			if (error_frames.find(current_state) != error_frames.end()){
				f = error_frames[current_state];
					
				//periodically update f->tp to reduce framerate of codecs
				//if camera not ready now
				boost::chrono::steady_clock::time_point now_tp = boost::chrono::steady_clock::now();
				typedef boost::chrono::duration<long,boost::milli> milliseconds;
				milliseconds ms = boost::chrono::duration_cast<milliseconds>(now_tp - f->tp);
				if (ms.count() > 300){
					//update f->tp
					f->tp = boost::chrono::steady_clock::now();
				}

				//reduce framerate
				if (f->tp <= last_frame_tp)
					return ;

				frame_size ef_size(f->avframe->width,f->avframe->height);
				if (current_fsize != ef_size){
					//framesize changed. need render again
					error_frames.erase(current_state);
					f = frame_ptr();
				}
			}
			if (!f){
				frame_ptr src_frame = _frame_helper.generate_black_frame(current_fsize);
				
				//write error text
				filter fi;
				std::string title = get_state_title(current_state);
			

				f = frame_ptr(new frame());
				f->tp = boost::chrono::steady_clock::now();
				f->avframe = fi.apply_to_frame(src_frame->avframe,"drawtext=x=(w*0.5 - text_w*0.5):y=(h*0.5-line_h*0.5):fontsize=20:fontcolor=white:text='"+title+"':fontfile="+FontsPath+"couri.ttf:box=1:boxcolor=black");	
				f->rotated = true;
				f->capturer_state = current_state;
				error_frames[current_state] = f;
			}
			break;
		}
	}

	//apply filters
	
	if ((rotate_filter_string != "")||(main_filter_string != "")){
		if (rotate_filter_string!="")
			if (!f->rotated){
				frame_ptr f1(new frame());
				f1->tp = f->tp;
				f1->capturer_state = f->capturer_state;
				f1->avframe = rotate_filter.apply_to_frame(f->avframe,rotate_filter_string);
				f1->rotated = true;

				f = f1;
			}
		if (main_filter_string!=""){
			frame_ptr f1(new frame());
			f1->tp = f->tp;
			f1->capturer_state = f->capturer_state;
			f1->avframe = main_filter.apply_to_frame(f->avframe,main_filter_string);
			f = f1;

		}
	}
	



	last_frame_spinlock.lock();
	last_frame = f;
	last_frame_spinlock.unlock();

	_fptr = f;
}


void camera_container::i_thread_controls_doc_change(std::string prop_name,std::string prov_value){

	//build rotate filter
	std::string r_string;
	int rotate_angle = controls_doc->get_property<int>(cprops::controls_RotateAngle,0);
	if (rotate_angle!=0){
		std::ostringstream oss;
		oss<<"rotate="<<rotate_angle<<"*PI/180";
		r_string = oss.str();

	}

	//build main filter
	std::deque<std::string> filter_parts;
	bool f = controls_doc->get_property<int>(cprops::controls_DisplayCameraName,0) == 1;
	if (f){
		std::string cam_name = controls_doc->get_property<std::string>(cprops::controls_CameraName,"");
		if (cam_name.size() != 0){
			std::string ss = "drawtext=x=(w*0.02):y=(h-text_h/2-line_h):fontsize=20:fontcolor=white:text='"+cam_name+"':fontfile="+FontsPath+"couri.ttf:box=1:boxcolor=black";
			filter_parts.push_back(ss);
		}

	}
	f = controls_doc->get_property<int>(cprops::controls_WriteDateTimeText,0) == 1;
	if (f){
		std::string ss = "drawtext=x=(w - text_w - w*0.02):y=(h-text_h/2-line_h):fontsize=20:fontcolor=white:text=%{localtime}:fontfile="+FontsPath+"couri.ttf:box=1:boxcolor=black";
		filter_parts.push_back(ss);
	}

	std::string mf_string = "",fp;
	BOOST_FOREACH(fp,filter_parts){
		if (mf_string != "")
			mf_string += ",";
		mf_string += fp;
	}

	if ((mf_string != main_filter_string)||(r_string != rotate_filter_string)){
		//need rebuild filters
		std::cout<<"need rebuild filters"<<std::endl;

		last_frame_mutex.lock();
		main_filter_string = mf_string;
		rotate_filter_string = r_string;
		last_frame_mutex.unlock();
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

std::string get_state_title(capturer::state st){
	std::string title;
	switch(st){
	default:
		case capturer::st_CaptureError:
			title = "Camera error";
			break;
		case capturer::st_InitializationError:
			title = "Camera startup error";
			break;
		case capturer::st_ConnectError:
			title = "No connection";
			break;
		case capturer::st_Initialization:
			title = "Connection...";
			break;
		case capturer::st_Setup:
			title = "Setting up...";
			break;
	}
	return title;
}

frame_ptr camera_container::find_error_frame(const frame_size fs,capturer::state c_st) const{
	BOOST_FOREACH(frame_ptr f,error_frames_d){
		if ((f->avframe->width != fs.width)||(f->avframe->height != fs.height))
			continue;
		if (f->capturer_state != c_st)
			continue;

		return f;
	}
	return frame_ptr();
}

void camera_container::i_thread_do_frame_generator_cleanup(boost::system::error_code ec){
	if (ec)
		return ;
	_frame_generator.cleanup(boost::posix_time::seconds(FRAME_GENERATOR_CLEANUP_TIMEOUT));

	frame_generator_cleanup_timer.expires_from_now(boost::chrono::seconds(5));
	frame_generator_cleanup_timer.async_wait(boost::bind(&camera_container::i_thread_do_frame_generator_cleanup,
		this,boost::asio::placeholders::error));
}



