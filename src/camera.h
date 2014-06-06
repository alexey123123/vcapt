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





class camera: public capturer{
public:
	
	enum type{
		c_local,
		c_network
	};
	typedef boost::function<void (std::string)> stop_handler;
	typedef boost::function<void (capturer::state,capturer::state)> state_change_handler;


	camera(const capturer::connect_parameters& _cp,state_change_handler _state_h, stop_handler _stop_h);
	virtual ~camera();


	//TODO:
	void set_connect_parameters(const connect_parameters& _cp)
		{conn_params = _cp;};
	const capturer::connect_parameters& get_connect_parameters() const
		{return conn_params; };


	void restart_connection()
		{do_disconnect_camera_device();};

protected:


	void main_doc_deleted();
	void runtime_doc_changed(std::string,std::string,std::string);
	void controls_doc_changed(std::string,std::string,std::string);

	void on_state_change(capturer::state old_state,capturer::state new_state);
private:
	
	capturer::connect_parameters conn_params;
	couchdb::manager* cdb_manager;
	stop_handler _stop_handler;
	state_change_handler _state_change_handler;
	Utility::Journal* journal;

	void start_connection(unsigned int _delay_ms);
	void finalize();
	bool finalization;

	boost::thread service_thread;
	boost::asio::io_service service_ioservice;
	boost::shared_ptr<boost::asio::io_service::work> service_ioservice_work_ptr;
	void service_thread_proc();

	void do_connect_camera_device(boost::system::error_code ec);
	boost::asio::steady_timer connect_camera_st;
	unsigned int connect_attempts_count;



	//call for network cams only, when url changed
	void do_disconnect_camera_device();

	void do_change_framesize(frame_size fs);

	void get_set_capture_options();



	//void sync_framesizes(const Capturer::Capabilities& caps, Capturer* capturer);
	//void do_change_framesize(FrameSize fsize);

	//filter
// 	Filter filter;
// 	std::string filter_string;
// 	void rebuild_filter_string(boost::system::error_code ec);
// 	boost::asio::steady_timer rebuild_filter_string_timer;


};





#endif//__camera_h__