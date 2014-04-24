#ifndef __camera_h__
#define __camera_h__


#include <queue>
#include <deque>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/thread.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/atomic.hpp>
#include <boost/chrono/duration.hpp>


#include <utility/Journal.h>
#include <utility/Options.h>

#include "types.h"
#include "couchdb.h"
#include "capturer.h"

typedef boost::function<void (std::string)> StopCameraHandler;

namespace CameraDocumentProps{
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




class camera: public capturer{
public:
	
	enum type{
		c_local,
		c_network
	};
	typedef boost::function<void (std::string)> stop_handler;

	camera(type t, const std::string& _dev_name_or_doc_id, couchdb::manager* _cdb_manager,stop_handler _h);
	virtual ~camera();

	void start_connection(unsigned int _delay_ms);


	std::string get_camera_unique_id()
		{return camera_unique_id;};

	std::string get_local_camera_devname()
		{return dev_name_or_doc_id;};


protected:
	void main_doc_changed(std::string,std::string,std::string);
	void main_doc_deleted();
	void runtime_doc_changed(std::string,std::string,std::string);
	void controls_doc_changed(std::string,std::string,std::string);

	void on_state_change(capturer::state old_state,capturer::state new_state);
private:
	type _type;
	std::string dev_name_or_doc_id;
	couchdb::manager* cdb_manager;
	stop_handler _stop_handler;
	Utility::Journal* journal;

	void finalize();

	bool check_and_create_documents2(couchdb::manager* dmanager,
		const std::string& camera_unique_id,
		std::string& error_message);


	couchdb::document_ptr main_doc;
	couchdb::document_ptr runtime_doc;
	couchdb::document_ptr controls_doc;
	couchdb::document::ticket_ptr main_doc_ticket;
	couchdb::document::ticket_ptr runtime_doc_ticket;
	couchdb::document::ticket_ptr controls_doc_ticket;







	std::string camera_unique_id;



	boost::thread service_thread;
	boost::asio::io_service service_ioservice;
	boost::shared_ptr<boost::asio::io_service::work> capture_ioservice_work_ptr;
	void service_thread_proc();



	couchdb::document_ptr find_local_camera_document(const std::string& camera_id,std::string& error_message);

	void do_connect_camera_device(boost::system::error_code ec);
	boost::asio::steady_timer connect_camera_st;





	//call for network cams only, when url changed
	void do_disconnect_camera_device();


	void get_set_capture_options();



	//void sync_framesizes(const Capturer::Capabilities& caps, Capturer* capturer);
	//void do_change_framesize(FrameSize fsize);

	//filter
// 	Filter filter;
// 	std::string filter_string;
// 	void rebuild_filter_string(boost::system::error_code ec);
// 	boost::asio::steady_timer rebuild_filter_string_timer;


};


typedef boost::shared_ptr<camera> CameraPtr;


#endif//__camera_h__