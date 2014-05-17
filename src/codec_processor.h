#ifndef __codec_processor_h__
#define __codec_processor_h__

#include <boost/filesystem/path.hpp>
#include <boost/thread.hpp>

#include "camera.h"
#include "types.h"
#include "codec.h"
#include "stream.h"

class codec_processor{
public:

	typedef boost::function<void (codec_processor*)> stop_handler; 

	codec_processor(camera* _cam, codec_ptr _cod,stop_handler _sh);
	~codec_processor();

	codec_ptr get_codec()
		{return _codec;};


	bool try_to_add_network_client(client_parameters e_params, tcp_client_ptr tcptr);	//creates av_container_stream or mjpeg_stream

	void add_file_stream(const boost::filesystem::path& p,		//creates av_container_stream(file)
		boost::chrono::steady_clock::duration _max_duration,
		boost::int64_t _max_filesize);							



	void delete_stream(stream* s);

private:
	camera* _camera;
	codec_ptr _codec;
	stop_handler _stop_handler;
	std::deque<stream_ptr> streams;
	
	boost::mutex internal_mutex;
	boost::asio::io_service internal_ioservice;
	boost::thread internal_thread;
	void internal_thread_proc();
	boost::shared_ptr<boost::asio::io_service::work> internal_ioservice_work_ptr;

	void i_thread_delete_stream(stream* s);



	void do_processor_work(boost::system::error_code ec);
	boost::asio::steady_timer work_timer;
	boost::chrono::steady_clock::time_point last_frame_tp;

	void check_clients_and_finalize_processor(boost::system::error_code ec);
	boost::asio::steady_timer check_clients_timer;

	void do_fps_meter(boost::system::error_code ec);
	boost::asio::steady_timer fps_meter_timer;
	// Calculated values
	uint64_t summary_encode_time_ms;
	uint64_t maximum_encode_time_ms;
	uint64_t minimum_encode_time_ms;
	int encoded_frame_counter;
	int encoder_fps;

	int exceptions_count;
	capturer::frame_ptr blank_frame;

};
typedef boost::shared_ptr<codec_processor> codec_processor_ptr;



#endif//__codec_processor_h__