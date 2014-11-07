#ifndef __camera_container_h__
#define __camera_container_h__

#include <boost/shared_ptr.hpp>

#include "camera.h"
#include "stream.h"
#include "codec_processor.h"
#include "archive_client.h"
#include "filter.h"
#include "draw.h"

class camera_container{
public:

	typedef boost::function<void (camera_container*)> stop_handler;

	camera_container(couchdb::manager* _cdb_manager, archive_client* _a_client, couchdb::document_ptr d, stop_handler sh);
	~camera_container();

	void add_network_client(tcp_client_ptr c);

	void delete_codec_processor(codec_processor* p);
	camera* get_camera()
		{return _camera;};
	std::string camera_id()
		{return main_doc->id();};

	//used by codec_processors
	void get_frame(boost::chrono::steady_clock::time_point last_frame_tp, frame_ptr& _fptr);

	enum frame_state{
		fs_Ok,
		fs_CapturerNotInitialized,
		fs_FramesizeOrFormatDiffers,
	};

 	frame_state get_frame2(
 		frame_size fs,
 		AVPixelFormat fmt,
 		boost::chrono::steady_clock::time_point last_frame_tp, 
 		frame_ptr& _fptr);
protected:
	void doc_changed(std::string,std::string,std::string);
	void doc_deleted();


	void stop_camera_handler(std::string stop_reason);
	void camera_state_change_handler(capturer::state old_state,capturer::state new_state);
private:
	couchdb::manager* cdb_manager;
	archive_client* a_client;
	stop_handler _stop_handler;
	camera* _camera;
	std::deque<codec_processor_ptr> codec_processors;

	frame_helper _frame_helper;

	frame_ptr last_frame;
	spinlock last_frame_spinlock;
	boost::mutex last_frame_mutex;
	std::map<capturer::state, frame_ptr> error_frames;

	std::deque<frame_ptr> error_frames_d;
	frame_ptr find_error_frame(const frame_size fs,capturer::state c_st) const;

	filter main_filter;
	std::string main_filter_string;
	filter rotate_filter;
	std::string rotate_filter_string;



	boost::mutex internal_mutex;
	boost::asio::io_service internal_ioservice;
	boost::thread internal_thread;
	void internal_thread_proc();
	boost::shared_ptr<boost::asio::io_service::work> internal_ioservice_work_ptr;

	void i_thread_delete_codec_processor(codec_processor* cp);

	couchdb::document_ptr			main_doc;
	couchdb::document::ticket_ptr	main_doc_tptr;

	couchdb::document_ptr			runtime_doc;	

	couchdb::document_ptr			controls_doc;
	couchdb::document::ticket_ptr	controls_doc_tptr;
	void i_thread_controls_doc_change(std::string,std::string);
	camera_controls					controls;	
	
	couchdb::document_ptr streams_doc;
	couchdb::document::ticket_ptr streams_doc_tptr;
	void i_thread_dispatch_streams_doc_change(boost::system::error_code ec);

	stream_ptr current_file_stream;
	stream_ptr next_file_stream;
	filestream_params f_params;
	void dispatch_answer_from_archive_server(boost::system::error_code ec,
		long_writing_file_ptr fptr,
		const filestream_params fp);
	boost::asio::steady_timer retry_filestream_start_timer;
	void i_thread_create_filestream(long_writing_file_ptr fptr,
		const filestream_params fp);
	void filestream_stop_handler(stream* s);
	void i_thread_stop_filestream(stream* s);
	void filestream_progress_handler(stream*,
		boost::system::error_code,
		boost::chrono::steady_clock::duration,	//current duration
		boost::int64_t);
	void i_thread_do_filestream_length_control(boost::system::error_code ec);
	boost::asio::steady_timer filestream_length_control_timer;
	bool request_to_next_delivered;

	//network cams
	void i_thread_dispatch_url_change(std::string new_url);

	void i_thread_dispatch_framesize_change(frame_size fs);

	frame_generator _frame_generator;
	boost::asio::steady_timer frame_generator_cleanup_timer;
	void i_thread_do_frame_generator_cleanup(boost::system::error_code ec);

	draw _draw;


};

#endif//__camera_container_h__