#ifndef __types_h__
#define __types_h__

#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/optional.hpp>
#include <boost/atomic/atomic.hpp>
#include <boost/shared_array.hpp>
#include <boost/tuple/tuple.hpp>


#include <opencv2/opencv.hpp>

#include <system/Platform.h>

#include "libav.h"
#include "couchdb.h"


struct frame_size{
	int width;
	int height;
	
	frame_size():width(0),height(0){};
	frame_size(int _width,int _height):width(_width),height(_height){};

	const bool operator==(const frame_size& fs) const{
		return (height==fs.height)&&(width==fs.width);
	};
	const bool operator!=(const frame_size& fs) const{
		return (!(*this == fs));
	};
	const bool operator<(const frame_size& fs) const{
		return width*height < fs.width*fs.height;
	};
	const bool operator>(const frame_size& fs) const{
		return width*height > fs.width*fs.height;
	};
	const bool operator<=(const frame_size& fs) const{
		return width*height <= fs.width*fs.height;
	};
	const bool operator>=(const frame_size& fs) const{
		return width*height >= fs.width*fs.height;
	};
	const int operator-(const frame_size& fs){
		return width*height - fs.width*fs.height;
	};

	std::string to_string() const;
	bool from_string(const std::string& s);
	void from_string(const std::string& s,const frame_size& default_value);

	operator bool() const{
		return (width!=0)&&(height!=0);
	};
};

struct __fs_sorter{
	bool operator()(const frame_size& fs1, const frame_size& fs2) {
		return (fs1.width*fs1.height) < (fs2.width*fs2.height);
	};
};




#define CLIENT_BUF_SIZE 1024
struct tcp_client{
	unsigned short port;
	boost::asio::ip::tcp::socket _socket;
	bool internal_client;
	
	
	char recv_buffer[CLIENT_BUF_SIZE];
	size_t received_bytes;

	std::deque<std::string> http_request_headers;

	std::string http_method;
	std::string http_url;
	std::string http_proto;

	std::map<std::string,std::string> url_keypairs;

	//...
	tcp_client(boost::asio::io_service& __ios):_socket(__ios){};
	boost::asio::ip::tcp::socket& get_socket()
			{return _socket;};
	~tcp_client();
};
typedef boost::shared_ptr<tcp_client> tcp_client_ptr;

struct client_parameters{
	std::string container_name;
	AVCodecID codec_id;
	boost::optional<frame_size> f_size;
	boost::optional<int> bitrate;

	boost::optional<int> autostop_timeout_sec;//autostop stream in cause of capturing&encoding errors


	bool construct(tcp_client_ptr,std::string& error_message);
	bool construct(couchdb::document_ptr d,std::string& error_message);
	client_parameters():codec_id(AV_CODEC_ID_NONE),autostop_timeout_sec(false){};
	bool operator==(const client_parameters& e) const
		{return (f_size==e.f_size)&&(bitrate==e.bitrate)&&(container_name==e.container_name)&&(codec_id==e.codec_id);};
	bool operator!=(const client_parameters& e) const
	{return !(e==*this);};
};

struct filestream_params{
	client_parameters c_params;
	boost::optional<boost::int64_t> max_filesize_mb;
	boost::optional<int> max_duration_mins;//(minutes)

	filestream_params(){};
	void from_doc(couchdb::document_ptr);

	boost::int64_t get_assumed_filesize() const ;

	bool operator==(const filestream_params& e) const
		{return (c_params==e.c_params)&&(max_filesize_mb==e.max_filesize_mb)&&(max_duration_mins==e.max_duration_mins);};
	bool operator!=(const filestream_params& e) const
		{return !(e==*this);};


};



#define PACKET_FLAG_CAPTURER_ERROR_BIT		1
#define PACKET_FLAG_STREAM_ERROR_BIT		2

struct packet{
	AVPacket p;
	boost::chrono::steady_clock::time_point frame_tp;

	std::vector<uchar> packet_data;//optional

	bool stream_header_data;
	
	int packet_flags;

	packet();
	~packet();
};
typedef boost::shared_ptr<packet> packet_ptr;

struct buffer{
	std::vector<unsigned char> _data;
	packet_ptr _packet_ptr;

	boost::posix_time::ptime pt;

	buffer();
	buffer(packet_ptr p);
	~buffer();

	std::size_t size() const;
	unsigned char* data();
};
typedef boost::shared_ptr<buffer> buffer_ptr;


enum file_state{
	fs_completed	= 0x00,
	fs_in_progress	= 0x01,
	fs_incompleted	= 0x02
};

namespace file_document_props{
	const std::string file_name		= "filename";
	const std::string file_path		= "filepath";
	const std::string file_state	= "state";
};

class file_document_wrapper{
public:
	file_document_wrapper(couchdb::document_ptr d);
	~file_document_wrapper();

	void mark_as_complete();
private:
	couchdb::document_ptr file_doc;
};


namespace cprops{
	//tags
	const std::string local_camera_tag = "local_camera";
	const std::string network_camera_tag = "network_camera";
	const std::string rpi_camera_tag = "rpi_camera";

	const std::string runtime_doc_id = "runtime_doc_id";
	const std::string controls_doc_id = "controls_doc_id";
	const std::string streams_doc_id = "streams_doc_id";


	const std::string unique_id = "unique_id";
	const std::string url = "url";
	const std::string network_camera_type = "net_camera_type";


	const std::string connect_error = "connect_error";
	const std::string camera_state = "connect_state";

	const std::string frame_size = "framesize";
	const std::string possible_framesizes = "framesizes";
	// 	const std::string MinFramesize = "min_framesize";
	// 	const std::string MaxFramesize = "max_framesize";


	const std::string controls_DisplayCameraName = "display_camera_name";
	const std::string controls_CameraName = "name";
	const std::string controls_WriteDateTimeText = "write_datetime";
	const std::string controls_RotateAngle = "rotate_angle";

	const std::string streams_save_video = "save_video";
	const std::string streams_video_max_size = "video_max_size_mb";
	const std::string streams_video_max_duration = "video_max_duration_nmins";
	const std::string streams_video_format = "video_format";
	const std::string streams_video_bitrate = "video_bitrate";
	const std::string streams_video_framesize = "video_framesize";


}

#if defined(LinuxPlatform)
const std::string FontsPath = "/usr/local/share/fonts/";
#elif defined(Win32Platform)
const std::string FontsPath = "";
#endif

struct camera_controls{
	boost::atomic<bool> need_display_camera_name;
	boost::atomic<char*> camera_name;
	boost::atomic<bool> need_display_timestamp;
	boost::atomic<int> rotate_angle;

	boost::atomic<char*> filter_string;

	camera_controls():need_display_camera_name(false),camera_name(0),need_display_timestamp(0),rotate_angle(0),filter_string(0){};
	~camera_controls();

	void update(couchdb::document_ptr d);

};


class spinlock {
private:
	typedef enum {Locked, Unlocked} LockState;
	boost::atomic<LockState> state_;

public:
	spinlock() : state_(Unlocked) {}

	void lock()
	{
		while (state_.exchange(Locked, boost::memory_order_acquire) == Locked) {
			/* busy-wait */
		}
	}
	void unlock()
	{
		state_.store(Unlocked, boost::memory_order_release);
	}
};

struct frame{
	AVFrame* avframe;
	boost::chrono::steady_clock::time_point tp;

	boost::shared_array<unsigned char> frame_data;

	int capturer_state;
	bool rotated;

	typedef boost::function<void ()> destroy_callback; 
	destroy_callback dcb;

	frame();
	~frame();		

	frame_size get_framesize() const;
};
typedef boost::shared_ptr<frame> frame_ptr;

class frame_helper{
public:
	frame_helper();
	~frame_helper();

	bool check_conversion(frame_size src_fsize,AVPixelFormat src_fmt,frame_size dst_fsize,AVPixelFormat dst_fmt,std::string& error_message) const;
	frame_ptr resize_and_convert(frame_ptr src, AVPixelFormat dst_fmt,frame_size dst_fsize, std::string& error_message);
	
	frame_ptr generate_black_frame(frame_size fs);
private:
	class Impl;
	Impl* pimpl;
};

class frame_generator{
public:
	frame_generator();

	frame_ptr get(frame_size fs, 
		const std::string& title, 
		int frame_interval_ms,
		boost::chrono::steady_clock::time_point last_frame_tp);

	//erasing frames which older than td
	void cleanup(boost::posix_time::time_duration td);
private:
	typedef boost::tuple<frame_size,std::string,frame_ptr> stored_frame;
	std::deque<stored_frame> ready_frames;
	frame_helper _frame_helper;

};


#endif//__types_h__