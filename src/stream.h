#ifndef __stream_h__
#define __stream_h__

#include <queue>

#include <boost/filesystem/path.hpp>
#include <boost/function.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

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



	void process_packet(packet_ptr p)
		{do_process_packet(p);};

	priority get_priority() const
		{return _priority;};
protected:

	virtual void do_process_packet(packet_ptr p) = 0;

	void call_stop_handler();
private:
	stop_handler _stop_handler;
	priority _priority;
	

};
typedef boost::shared_ptr<stream> stream_ptr;


class network_stream_buffer{
public:
	typedef boost::function<void (network_stream_buffer*,boost::system::error_code)> stop_handler;
	network_stream_buffer(tcp_client_ptr _tcptr,stop_handler _h, unsigned int max_buffer_size);
	~network_stream_buffer();

	void close();

	void add_buffer(buffer_ptr b);

private:
	tcp_client_ptr tcptr;
	stop_handler _stop_handler;
	unsigned int _max_buffer_size;

	std::queue<buffer_ptr> buffers;
	buffer_ptr current_buffer;
	boost::recursive_mutex buffers_mutex;
	std::vector<unsigned char>::size_type total_size;

	void send_first_buffer();
	void write_data_handler(buffer_ptr bptr, boost::system::error_code,unsigned int bytes_transferred);
	int sent_packets_count;
	boost::system::error_code client_error_code;
};

#define STREAM_BUFFER_SIZE 100000
class av_container_stream: public stream{
public:

	virtual ~av_container_stream();

	static stream_ptr create_network_stream(const std::string& container_name, AVCodecContext* av_codec_context, stream::stop_handler sh,
														tcp_client_ptr c);

	
	//file-stream progress handler
	//if return false - need stop stream
	typedef boost::function<bool (stream*,
									boost::system::error_code,
									boost::chrono::steady_clock::duration,
									boost::int64_t)> progress_handler;

 	static stream_ptr create_file_stream(const std::string& container_name, 
		AVCodec* av_codec, 
		stop_handler sh,
		progress_handler ph,
 		const boost::filesystem::path& p,
		boost::posix_time::ptime start_ptime,	//start timepoint (stream cannot start before this time)
 		boost::chrono::steady_clock::duration _max_duration,
 		boost::int64_t _max_filesize);



	//network stream
	int do_delivery_encoded_data(uint8_t *buf, int buf_size);
protected:
	void do_process_packet(packet_ptr p);
private:
	av_container_stream(priority p, stop_handler sh,tcp_client_ptr c);

	AVFormatContext		*format_context;	
	AVStream			*video_stream;
	AVCodecContext*		fake_context;
	boost::chrono::steady_clock::time_point stream_start_tp;

	

	void alloc_stream(const std::string& container_name, AVCodecContext* av_codec_context);

	//file stream stuff
	boost::filesystem::path file_path;
	boost::chrono::steady_clock::duration max_duration;
	boost::int64_t max_filesize;
	progress_handler _progress_handler;


	//network stream stuff
	unsigned char out_stream_buffer[STREAM_BUFFER_SIZE];

	int64_t do_seek(int64_t offset, int whence);
	network_stream_buffer nsb;
	void nsb_finalize(network_stream_buffer* _nsb, boost::system::error_code ec);	

	std::string get_http_ok_answer(const std::string& container_name);
	bool kf_found;
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