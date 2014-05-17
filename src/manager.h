#ifndef __manager_h__
#define __manager_h__

#include <deque>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/steady_timer.hpp>

#include <utility/Options.h>

#include "types.h"
#include "couchdb.h"
#include "camera_opencv.h"
#include "camera_v4l2.h"
#include "camera_container.h"


class manager{
public:
	manager(Utility::Options* options);
	~manager();

	//local devices scan
	//network cameras docs scan
	//wait incoming connections

	void finalize_codec_processor(camera_container* _container, codec_processor* _processor);
private:
	couchdb::manager cdb_manager;
	boost::asio::io_service internal_ioservice;
	boost::shared_ptr<boost::asio::io_service::work> internal_ioservice_work_ptr;
	boost::thread internal_thread;
	void internal_thread_proc();

	//local devices scanner
	boost::asio::steady_timer local_devices_scan_timer;
	void do_local_devices_scan(boost::system::error_code ec);
	std::map<std::string,int> videodevices;
	void do_videodev_appearance(const std::string& devname);
	void do_videodev_disappearance(const std::string& devname);

	//network documents chage handler
	void network_camera_doc_changed(const std::string& doc_id, const boost::property_tree::ptree& document_ptree);
	void i_thread_network_camera_doc_changed(std::string doc_id, boost::property_tree::ptree document_ptree);



	//income connections acceptor
	boost::asio::ip::tcp::acceptor u_acceptor;
	boost::asio::ip::tcp::acceptor s_acceptor;
	void start_acceptor(boost::asio::ip::tcp::acceptor& a, unsigned short port);
	void handle_tcp_accept(unsigned short port, tcp_client_ptr cptr,boost::system::error_code ec);
	void handle_receive_bytes(tcp_client_ptr client,boost::system::error_code ec,size_t bytes_transferred);
	void dispatch_new_client(tcp_client_ptr client);





	typedef boost::shared_ptr<camera_container> camera_container_ptr;
	std::deque<camera_container_ptr> cameras;
	void on_camera_finalize(std::string camera_id);



	void i_thread_finalize_codec_processor(camera_container* _container, codec_processor* _processor);

	void check_database_and_start_network_cams();
};

#endif//__manager_h__