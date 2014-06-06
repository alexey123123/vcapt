#ifndef __camera_container_h__
#define __camera_container_h__

#include <boost/shared_ptr.hpp>

#include "camera.h"
#include "stream.h"
#include "codec_processor.h"

namespace cprops{
	//tags
	const std::string LocalCameraTag = "local_camera";
	const std::string NetworkCameraTag = "network_camera";

	const std::string RuntimeDocId = "runtime_doc_id";
	const std::string ControlsDocId = "controls_doc_id";
	const std::string UniqueId = "unique_id";

	const std::string Url = "url";


	const std::string NetworkCameraType = "net_camera_type";


	const std::string ConnectError = "connect_error";
	const std::string CameraState = "connect_state";

	const std::string FrameSize = "framesize";
	const std::string PossibleFrameSizes = "framesizes";
	const std::string MinFramesize = "min_framesize";
	const std::string MaxFramesize = "max_framesize";


	const std::string controls_DisplayCameraName = "display_camera_name";
	const std::string controls_CameraName = "name";
	const std::string controls_WriteDateTimeText = "write_datetime";
	const std::string controls_RotateAngle = "rotate_angle";

}



class camera_container{
public:

	typedef boost::function<void (camera_container*)> stop_handler;

	camera_container(couchdb::manager* _cdb_manager, couchdb::document_ptr d, stop_handler sh);
	~camera_container();

	void add_network_client(tcp_client_ptr c);

	void delete_codec_processor(codec_processor* p);
	camera* get_camera()
		{return _camera;};
	std::string camera_id()
		{return main_doc->id();};
protected:
	void camera_main_doc_changed(std::string,std::string,std::string);
	void camera_main_doc_deleted();

	void stop_camera_handler(std::string stop_reason);
	void camera_state_change_handler(capturer::state old_state,capturer::state new_state);
private:
	couchdb::manager* cdb_manager;
	stop_handler _stop_handler;
	camera* _camera;
	std::deque<codec_processor_ptr> codec_processors;

	boost::mutex internal_mutex;
	boost::asio::io_service internal_ioservice;
	boost::thread internal_thread;
	void internal_thread_proc();
	boost::shared_ptr<boost::asio::io_service::work> internal_ioservice_work_ptr;

	void i_thread_delete_codec_processor(codec_processor* cp);

	couchdb::document_ptr main_doc;
	couchdb::document::ticket_ptr main_doc_tptr;
	couchdb::document_ptr runtime_doc;
	couchdb::document_ptr streams_doc;

	void i_thread_dispatch_url_change(std::string new_url);

};

#endif//__camera_container_h__