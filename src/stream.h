#ifndef __stream_h__
#define __stream_h__

#include <queue>
#include <fstream>

#include <boost/filesystem/path.hpp>
#include <boost/function.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/atomic/atomic.hpp>

#include "libav.h"
#include "types.h"

class stream{
public:

	typedef boost::function<void (stream*)> stop_handler;
	enum priority{
		p_unknown			= 0,
		p_user				= 10,
		p_system_service	= 20,
		p_save_to_file		= 30
	};


	stream(priority p, stop_handler sh);
	virtual ~stream();

	void start();
	bool started() const;
	

	void process_packet(packet_ptr p)
		{do_process_packet(p);};

	priority get_priority() const
		{return _priority;};

	//
	void signal_to_stop()
		{call_stop_handler("");};

	int get_duration_secs() const;
	boost::int64_t get_written_bytes() const;

protected:

	virtual void do_process_packet(packet_ptr p) = 0;

	void call_stop_handler(const std::string& reason = "");
	
	boost::chrono::steady_clock::time_point stream_start_tp;
	boost::posix_time::ptime stream_start_ptime;

	bool stopping() const;

	boost::atomic<boost::int64_t> bytes_written_at;
private:
	stop_handler _stop_handler;
	priority _priority;
	bool _stopping;


};
typedef boost::shared_ptr<stream> stream_ptr;


class network_stream_buffer{
public:
	typedef boost::function<void (network_stream_buffer*,boost::system::error_code)> stop_handler;
	network_stream_buffer(tcp_client_ptr _tcptr,stop_handler _h, unsigned int max_buffer_size);
	network_stream_buffer(){};
	~network_stream_buffer();

	void close();

	void add_buffer(buffer_ptr b);

	bool defined() const
		{return tcptr != tcp_client_ptr();};
private:
	tcp_client_ptr tcptr;
	stop_handler _stop_handler;
	unsigned int _max_buffer_size;

	std::queue<buffer_ptr> buffers;
	buffer_ptr current_buffer;
	buffer_ptr next_buffer;
	boost::recursive_mutex buffers_mutex;
	std::vector<unsigned char>::size_type total_size;

	void send_first_buffer();
	void write_data_handler(buffer_ptr bptr, boost::system::error_code,unsigned int bytes_transferred);
	int sent_packets_count;
	boost::system::error_code client_error_code;

	boost::posix_time::ptime last_debug_output_pt;
	unsigned int total_delivery_time_ms;
	unsigned int deliveries_count;



};

#define STREAM_BUFFER_SIZE 100000
class av_container_stream: public stream{
public:

	virtual ~av_container_stream();

	static stream_ptr create_network_stream(const client_parameters& c_params, 
		AVCodecContext* av_codec_context, 
		stream::stop_handler sh,
		tcp_client_ptr c);

	
	//file-stream progress handler
	//if return false - need stop stream
	typedef boost::function<void (stream*,
									boost::system::error_code,
									boost::chrono::steady_clock::duration,	//current duration
									boost::int64_t) //current filesize
							> progress_handler;		

	static stream_ptr create_file_stream(
		filestream_params fstream_params, 
		AVCodecContext* av_codec_context, 
		stream::stop_handler sh,
		progress_handler ph,
		const boost::filesystem::path& p
		);



	//network stream
	int do_write(uint8_t *buf, int buf_size);
	int64_t do_seek(int64_t offset, int whence);
protected:
	void do_process_packet(packet_ptr p);
private:
	av_container_stream(priority p, stop_handler sh,tcp_client_ptr c);
	av_container_stream(stop_handler sh);

	AVFormatContext		*format_context;	
	AVStream			*video_stream;

	void alloc_stream(const std::string& container_name, AVCodecContext* av_codec_context);


	


	//file stream stuff
	std::ofstream file_stream;
	boost::filesystem::path file_path;
	filestream_params fs_params;
	progress_handler _progress_handler;


	//network stream stuff
	unsigned char out_stream_buffer[STREAM_BUFFER_SIZE];

	tcp_client_ptr _tcp_client;

	//network_stream_buffer nsb;
	void nsb_finalize(network_stream_buffer* _nsb, boost::system::error_code ec);	

	std::string get_http_ok_answer(const std::string& container_name);
	bool kf_found;

// 	bool its_a_network_stream() const
// 		{return nsb.defined();};
	bool its_a_network_stream() const
	 		{return _tcp_client.get() != 0;};
	bool its_a_file_stream() const
		{return !its_a_network_stream();};


	int current_delay;
	boost::optional<int> autostop_timeout;
	typedef boost::optional<boost::chrono::steady_clock::time_point> TP;
	TP first_error_frame_tp;



};

class mjpeg_stream: public stream{
public:
	mjpeg_stream(stop_handler sh,tcp_client_ptr c);
protected:
	void do_process_packet(packet_ptr p);
private:
	network_stream_buffer nsb;
	void nsb_finalize(network_stream_buffer* _nsb, boost::system::error_code ec);
	std::string boundary_value;

};


#endif//__stream_h__