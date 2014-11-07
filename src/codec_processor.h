#ifndef __codec_processor_h__
#define __codec_processor_h__

#include <boost/filesystem/path.hpp>
#include <boost/thread.hpp>

#include "types.h"
#include "codec.h"
#include "stream.h"

class camera_container;

class codec_processor{
public:

	typedef boost::function<void (codec_processor*)> stop_handler; 

	codec_processor(camera_container* _ccont, codec_ptr _cod,stop_handler _sh);
	~codec_processor();

	codec_ptr get_codec()
		{return _codec;};


	bool try_to_add_network_client(client_parameters e_params, tcp_client_ptr tcptr);	//creates av_container_stream or mjpeg_stream

	void add_stream(stream_ptr sptr);
	void delete_stream(stream* s);

	static AVCodecID select_codec_for_container(const std::string& cont_name);

private:
	camera_container* _camera_container;
	codec_ptr _codec;
	stop_handler _stop_handler;
	std::deque<stream_ptr> streams;
	boost::atomic<int> streams_count;

	boost::mutex internal_mutex;
	boost::asio::io_service internal_ioservice;
	boost::thread internal_thread;
	void internal_thread_proc();
	boost::shared_ptr<boost::asio::io_service::work> internal_ioservice_work_ptr;

	void i_thread_delete_stream(stream* s);
	void i_thread_add_stream(stream_ptr sptr);



	void do_processor_work(boost::system::error_code ec);
	boost::asio::steady_timer work_timer;
	boost::chrono::steady_clock::time_point last_frame_tp;

	void check_clients_and_finalize_processor(boost::system::error_code ec);
	boost::asio::steady_timer check_clients_timer;

	void do_fps_meter(boost::system::error_code ec);
	boost::asio::steady_timer fps_meter_timer;
	// Calculated values
	uint64_t sum_value;
	uint64_t max_value;
	uint64_t min_value;
	uint64_t last_packet_pts;

	int encoded_frame_counter;
	int encoder_fps;
	int empty_get_frame_calls;

	uint64_t summary_gf2_time_ms;
	uint64_t summary_gf2_count;
	unsigned int gf2_average;//average get_frame2 duration


	int exceptions_count;
	frame_ptr blank_frame;

	void call_stop_handler(const std::string& reason);

	int last_error_fs;
	std::string last_error_title;
	frame_generator _frame_generator;
	void i_thread_frame_generator_cleanup(boost::system::error_code ec);
	boost::asio::steady_timer frame_generator_cleanup_timer;

};
typedef boost::shared_ptr<codec_processor> codec_processor_ptr;



#endif//__codec_processor_h__