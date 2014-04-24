#include <boost/lexical_cast.hpp>


#include <utility/Pack.h>

#include "types.h"



std::string frame_size::to_string() const{
	std::ostringstream oss;
	oss << width << "x" << height;
	return oss.str();
}
bool frame_size::from_string(const std::string& s){
	try{
		std::deque<std::string> parts = Utility::Pack::__parse_string_by_separator__(s,"x");
		if (parts.size() != 2)
			throw std::runtime_error("");

		width = boost::lexical_cast<int>(parts[0]);
		height = boost::lexical_cast<int>(parts[1]);

		return true;
	}
	catch(...){

	}
	return false;

}


buffer::buffer():av_packet(0){};
buffer::buffer(AVPacket* p):av_packet(p){};
buffer::~buffer(){
	if (av_packet!=0)
		libav::av_free_packet(av_packet);
}

std::size_t buffer::size() const{
	if (av_packet != 0)
		return av_packet->size;
	return _data.size();
};

unsigned char* buffer::data(){
	if (av_packet != 0)
		return av_packet->data;
	return &_data[0];

}


tcp_client::~tcp_client(){
	boost::system::error_code ec;
	_socket.close(ec);
};

bool client_parameters::construct(tcp_client_ptr cptr,std::string& error_message){
	try{
		if (cptr->url_keypairs.find("type")==cptr->url_keypairs.end())
			throw std::runtime_error("undefined stream type");

		//	container/codec
		std::string stream_type_str = cptr->url_keypairs["type"];
		std::deque<std::string> parts = Utility::Pack::__parse_string_by_separator__(stream_type_str,"/");
		if (parts.size() != 2)
			throw std::runtime_error("cannot decode stream type (must be: container/codec)");
		container_name = parts[0];
		codec_name = parts[1];


		int width = 0;
		if (cptr->url_keypairs.find("width")!=cptr->url_keypairs.end())
			try{
				width = boost::lexical_cast<int>(cptr->url_keypairs["width"]);
		}
		catch(...){
			throw std::runtime_error("width is incorrected");
		}

		int height = 0;
		if (cptr->url_keypairs.find("height")!=cptr->url_keypairs.end())
			try{
				height = boost::lexical_cast<int>(cptr->url_keypairs["height"]);
		}
		catch(...){
			throw std::runtime_error("width is incorrected");
		}

		if ((width != 0)&&(height!=0))
			f_size = frame_size(width,height);

		f_size = frame_size(width,height);


		bitrate = 0;
		if (cptr->url_keypairs.find("bitrate")!=cptr->url_keypairs.end())
			try{
				bitrate = boost::lexical_cast<int>(cptr->url_keypairs["bitrate"]);
			}
			catch(...){
				throw std::runtime_error("bitrate incorrected");
			}

		return true;
	}
	catch(std::runtime_error& ex){
		error_message = std::string(ex.what());
	}
	return false;


}
