#ifndef __camera_container_h__
#define __camera_container_h__

#include "stream.h"
#include "codec_processor.h"

class camera_container{
public:
	camera_container(camera* c);
	~camera_container();

	void add_network_client(tcp_client_ptr c);

	void delete_codec_processor(codec_processor* p);
	camera* get_camera()
		{return _camera;};
private:
	camera* _camera;
	std::deque<codec_processor_ptr> codec_processors;

	boost::mutex internal_mutex;
	boost::asio::io_service internal_ioservice;
	boost::thread internal_thread;
	void internal_thread_proc();
	boost::shared_ptr<boost::asio::io_service::work> internal_ioservice_work_ptr;

	void i_thread_delete_codec_processor(codec_processor* cp);


};

#endif//__camera_container_h__