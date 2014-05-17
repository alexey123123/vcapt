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
void frame_size::from_string(const std::string& s,const frame_size& default_value){
	try{
		std::deque<std::string> parts = Utility::Pack::__parse_string_by_separator__(s,"x");
		if (parts.size() != 2)
			throw std::runtime_error("");

		width = boost::lexical_cast<int>(parts[0]);
		height = boost::lexical_cast<int>(parts[1]);
	}
	catch(...){
		width = default_value.width;
		height = default_value.height;
	}
}


buffer::buffer(){};
buffer::buffer(packet_ptr p):_packet_ptr(p){};
buffer::~buffer(){

}

std::size_t buffer::size() const{
	if (_packet_ptr)
		return _packet_ptr->p.size;
	return _data.size();
};

unsigned char* buffer::data(){
	if (_packet_ptr != 0)
		return _packet_ptr->p.data;
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


		f_size = boost::optional<frame_size>();

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

		if ((width != 0)&&(height!=0)){
			f_size = boost::optional<frame_size>(frame_size(width,height));
			std::cout<<"fsize:"<<f_size->to_string()<<std::endl;
		}
			

		bitrate = boost::optional<int>();
		int b = 0;
		if (cptr->url_keypairs.find("bitrate")!=cptr->url_keypairs.end())
			try{
				b = boost::lexical_cast<int>(cptr->url_keypairs["bitrate"]);
				bitrate = boost::optional<int>(b);
				std::cout<<"bitrate:"<<b<<std::endl;
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

packet::packet(){

}
packet::~packet(){
	libav::av_free_packet(&p);
}



