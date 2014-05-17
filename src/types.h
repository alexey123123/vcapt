#ifndef __types_h__
#define __types_h__

#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/optional.hpp>


#include <opencv2/opencv.hpp>

#include "libav.h"


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
	std::string codec_name;
	boost::optional<frame_size> f_size;
	boost::optional<int> bitrate;


	bool construct(tcp_client_ptr,std::string& error_message);
	client_parameters(){};
	bool operator==(const client_parameters& e) const
		{return (f_size==e.f_size)&&(bitrate==e.bitrate)&&(container_name==e.container_name)&&(codec_name==e.codec_name);};
	bool operator!=(const client_parameters& e) const
	{return !(e==*this);};
};

struct packet{
	AVPacket p;
	boost::chrono::steady_clock::time_point frame_tp;

	packet();
	~packet();
};
typedef boost::shared_ptr<packet> packet_ptr;

struct buffer{
	std::vector<unsigned char> _data;
	packet_ptr _packet_ptr;

	buffer();
	buffer(packet_ptr p);
	~buffer();

	std::size_t size() const;
	unsigned char* data();
};
typedef boost::shared_ptr<buffer> buffer_ptr;



#endif//__types_h__