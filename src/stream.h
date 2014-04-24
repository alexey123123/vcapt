#ifndef __stream_h__
#define __stream_h__

#include <deque>
#include <memory>

#include <boost/shared_ptr.hpp>
#include <boost/system/error_code.hpp>


#include "types.h"
#include "camera.h"
#include "ffmpeg_encoder.h"
#include "filter.h"


/*
	- temp_connection (xxx.xxx.xxx.xxx/stream?type=xxx?id=xxx?fsize=xxx:xxx?)
	- feed (		xxx.xxx.xxx.xxx/stream/xxxxxxxxxxxxx
				xxx.xxx.xxx.xxx/pstream/xxxxxxxxxxxxx

*/


class manager;

class stream{
public:


	struct parameters{
		std::string codec_name;
		std::string container_name;
		ffmpeg_encoder::format encoder_format;
	};

	stream(manager* _m, camera* _c,boost::shared_ptr<ffmpeg_encoder> e_ptr, const parameters& p);
	~stream();


	camera* get_camera()
		{return _camera;};
	ffmpeg_encoder* get_encoder()
		{return _encoder_ptr.get();};
	parameters& get_params()
		{return _parameters;};

	bool have_priority_clients();
	bool have_clients();



	void add_new_tcp_client(tcp_client_ptr c);
	void remove_tcp_client(tcp_client_ptr c);

	struct client{
		tcp_client_ptr _tcp_client_ptr;
		stream* _stream;

		std::queue<buffer_ptr> buffers;
		buffer_ptr current_buffer;
		boost::mutex buffers_mutex;
		std::vector<unsigned char>::size_type total_size;
		void add_buffer(buffer_ptr b);
		void send_first_buffer();
		void write_data_handler(buffer_ptr bptr, boost::system::error_code,unsigned int bytes_transferred);

		int sent_packets_count;
		bool header_sended;

		boost::system::error_code client_error_code;


		bool mjpeg_answer_delivered;


		client(stream* s,tcp_client_ptr c):
		_tcp_client_ptr(c),
			_stream(s),
			total_size(0),sent_packets_count(0),
			mjpeg_answer_delivered(false),
			header_sended(false){};
		~client();

	};
	typedef boost::shared_ptr<client> client_ptr;


private:	
	manager* _manager;
	camera* _camera;
	boost::shared_ptr<ffmpeg_encoder> _encoder_ptr;
	parameters _parameters;

	boost::asio::io_service internal_ioservice;
	boost::shared_ptr<boost::asio::io_service::work> internal_ioservice_work_ptr;
	boost::thread internal_thread;
	void internal_thread_proc();

	void do_stream_work(boost::system::error_code ec);
	boost::asio::steady_timer work_timer;

	void check_clients_and_finalize_stream(boost::system::error_code ec);
	boost::asio::steady_timer check_clients_timer;

	void do_fps_meter(boost::system::error_code ec);
	boost::asio::steady_timer fps_meter_timer;
	// Calculated values
	uint64_t summary_encode_time_ms;
	uint64_t maximum_encode_time_ms;
	uint64_t minimum_encode_time_ms;
	int encoded_frame_counter;
	int encoder_fps;




	std::deque<client_ptr> clients;
	boost::mutex clients_mutex;
	void i_thread_add_new_tcp_client(tcp_client_ptr c);
	void i_thread_remove_tcp_client(tcp_client_ptr c);

	void encoder_data_handler(buffer_ptr b);
	boost::chrono::steady_clock::time_point last_frame_tp;

	capturer::frame_ptr blank_frame;
	std::map<capturer::state, filter_ptr> s_filters;

};
typedef boost::shared_ptr<stream> stream_ptr;

#endif//__stream_h__