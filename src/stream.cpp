#include <boost/asio/write.hpp>
#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>

#include "stream.h"

stream::stream(priority p, stop_handler sh):_priority(p), _stop_handler(sh){
	
};

stream::~stream(){
}

void stream::call_stop_handler(){
	_stop_handler(this);
}



av_container_stream::av_container_stream(priority p, stop_handler sh,tcp_client_ptr c):stream(p,sh),
	fake_context(0),format_context(0),video_stream(0),
	nsb(c,boost::bind(&av_container_stream::nsb_finalize,this,_1,_2), 10000000),
	kf_found(false)
{
	stream_start_tp = boost::chrono::steady_clock::now();
}

av_container_stream::~av_container_stream(){
	std::cout<<"~av_container_stream()"<<std::endl;

	nsb.close();

	if (format_context){

		//TODO: write_trailer

		if (format_context->pb != 0){
			libav::av_free(format_context->pb);
			format_context->pb = 0;
		}
		

		if (video_stream!= 0){
			video_stream->codec = fake_context;
			if (video_stream->codec != 0)
				libav::avcodec_close(video_stream->codec);
		}


// 		if (video_stream)
// 			video_stream->codec = 0;

		libav::avformat_free_context(format_context);

	}


}

void av_container_stream::nsb_finalize(network_stream_buffer* _nsb, boost::system::error_code ec){
	call_stop_handler();
}



int __write_packet__2(void *opaque, uint8_t *buf, int buf_size);



stream_ptr av_container_stream::create_network_stream(const std::string& container_name, 
														AVCodecContext* av_codec_context, 
														stream::stop_handler sh,
														tcp_client_ptr c){

	if (container_name=="mjpeg"){
		return boost::shared_ptr<mjpeg_stream>(new mjpeg_stream(sh,c));
	}

	av_container_stream* avs(new av_container_stream(
									c->internal_client ? stream::p_system_service : stream::p_user,
									sh,c)); 


		try{
			avs->alloc_stream(container_name,av_codec_context);


			//network stream stuff
			avs->format_context->pb = libav::avio_alloc_context(avs->out_stream_buffer,STREAM_BUFFER_SIZE,1,0,0,__write_packet__2,0);
			if (!avs->format_context->pb)
				throw std::runtime_error("avio_alloc_context error");
			avs->format_context->pb->opaque = (void*)avs;

			//generate http-answer
			std::string http_ok_answer = avs->get_http_ok_answer(container_name);
			c->get_socket().send(boost::asio::buffer(http_ok_answer));

			libav::avformat_write_header(avs->format_context,0);
		}
		catch(std::runtime_error& ex){			
			delete avs;
			avs = 0;

			throw std::runtime_error(ex.what());

		}
		
		return stream_ptr(avs);
}

void av_container_stream::alloc_stream(const std::string& container_name, AVCodecContext* av_codec_context){
	libav::avformat_alloc_output_context2(&format_context, NULL, container_name.c_str(), NULL);
	if (!format_context)
		throw std::runtime_error("cannot initialize container ("+container_name+")");
	
	video_stream = libav::avformat_new_stream(format_context, av_codec_context->codec);
	if (!video_stream)
		throw std::runtime_error("cannot create stream (codec incompatible with container?)");

	//save dumy codec context;
	fake_context = video_stream->codec;
	
	video_stream->codec = av_codec_context;

	video_stream->id = format_context->nb_streams-1;
	video_stream->index = format_context->nb_streams-1;
}



void av_container_stream::do_process_packet(packet_ptr p){
	//write packet to container

	AVPacket* avpacket = &(p->p);
	avpacket->stream_index = video_stream->id;


	avpacket->pts = boost::chrono::duration_cast<boost::chrono::milliseconds>(p->frame_tp - stream_start_tp).count();

	AVRational r1;
	r1.num = 1;
	r1.den = 1000;
	avpacket->pts = libav::av_rescale_q_rnd(avpacket->pts, r1, video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
	avpacket->dts = avpacket->pts;
	//avpacket->duration = libav::av_rescale_q(avpacket->duration, r1, video_stream->time_base);
	avpacket->duration = 30;

	

	//kf_found =  ? true

	if (!kf_found)
		if (avpacket->flags & AV_PKT_FLAG_KEY)
			kf_found = true;

	if (kf_found){
		//transfer packet to stream
		libav::av_write_frame(format_context,avpacket);
	}


}




network_stream_buffer::network_stream_buffer(tcp_client_ptr _tcptr,stop_handler _h, unsigned int max_buffer_size):
tcptr(_tcptr),_stop_handler(_h),_max_buffer_size(max_buffer_size),
total_size(0),sent_packets_count(0){

}
network_stream_buffer::~network_stream_buffer(){
	close();
}
void network_stream_buffer::close(){
	tcptr = tcp_client_ptr();
}

void network_stream_buffer::add_buffer(buffer_ptr b){
	boost::unique_lock<boost::recursive_mutex> l1(buffers_mutex);
	if (client_error_code)
		return ;
	while (total_size + b->size() >= _max_buffer_size){
		std::cout<<"add_buffer: need cleanup"<<std::endl;
		buffer_ptr b1 = buffers.front();
		if (b1){
			buffers.pop();
			total_size -= b1->size();
		} else
			break;
	}

	if (total_size + b->size() >= _max_buffer_size)
		throw std::runtime_error("NetworkStreamer: buffer overflow");

	buffers.push(b);
	total_size += b->size();
	// std::cout<<"B: total_size="<<total_size<<",count="<<buffers.size()<<std::endl;
	send_first_buffer();
}
void network_stream_buffer::send_first_buffer(){
	boost::unique_lock<boost::recursive_mutex> l1(buffers_mutex);
	if (!current_buffer){

		if (buffers.size()>0){
			current_buffer = buffers.front();
			buffers.pop();
		}

		if (current_buffer){
			boost::asio::async_write(tcptr->get_socket(),
				boost::asio::buffer(current_buffer->data(),current_buffer->size()),
				boost::bind(&network_stream_buffer::write_data_handler, 
				this,
				current_buffer,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
			sent_packets_count++;
		}

	}

}
void network_stream_buffer::write_data_handler(buffer_ptr bptr, boost::system::error_code ec,unsigned int bytes_transferred){
	boost::unique_lock<boost::recursive_mutex> l1(buffers_mutex);
	sent_packets_count--;
	if (!ec){
		std::cout<<"bytes_transferred:"<<bytes_transferred<<std::endl;
		if (bptr->size() != bytes_transferred)
			std::cout<<"------------TRANSFERED only "<<bytes_transferred<<" bytes from "<<bptr->size()<<std::endl;

		//send next data buffer

		if (current_buffer){
			total_size -= current_buffer->size();
			current_buffer = buffer_ptr();
		}

		send_first_buffer();

	} else{
		//deleting client
		std::string e_mess = ec.message();
		std::cout<<"delivery error (bt:"<<bytes_transferred<<")("<<e_mess<<")"<<std::endl;
		client_error_code = ec;


		//client disconnected
		_stop_handler(this,client_error_code);
	}

}

int av_container_stream::do_delivery_encoded_data(uint8_t *buf, int buf_size){
		//std::cout<<"Encoder::webm_write_packet("<<buf_size<<" bytes)"<<std::endl;
	if (buf_size==0)
		return 0;


	/*
	if (!test_fstream){
		std::string filename = "vcapt."+container_name;
		test_fstream.open(filename.c_str(),std::ios::out|std::ios::trunc|std::ios::binary);
	}
		
	if (test_fstream){
		test_fstream.write((const char*)buf,buf_size);
		std::cout<<"written "<<buf_size<<", tellp:"<<test_fstream.tellp()<<std::endl;
	}
	*/
		



	buffer_ptr b(new buffer());
	b->_data.reserve(buf_size);
	std::copy(buf,buf+buf_size,std::back_inserter(b->_data));
	nsb.add_buffer(b);

	return buf_size;
}
int64_t av_container_stream::do_seek(int64_t offset, int whence){
	return offset;
}



int __write_packet__2(void *opaque, uint8_t *buf, int buf_size){
	av_container_stream* s = (av_container_stream*)opaque;
	return s->do_delivery_encoded_data(buf,buf_size);
}

std::string av_container_stream::get_http_ok_answer(const std::string& container_name){
	std::ostringstream oss;
	oss <<"HTTP/1.0 200 OK"<<std::endl;
	oss <<"Pragma: no-cache"<<std::endl;
	if (container_name=="mjpeg"){
		oss << "Server: some device"<<std::endl;
		oss << "Accept-Ranges: bytes"<<std::endl;
		oss << "Connection: close"<<std::endl;
		oss << "Content-Type: multipart/x-mixed-replace; boundary=--ipcamera"<<std::endl;

	} else
		//oss <<"Content-Type: video/"<<container_name<<std::endl;
		oss <<"Content-Type: "<<format_context->oformat->mime_type<<std::endl;


	

	oss << std::endl;

	return oss.str();
}

mjpeg_stream::mjpeg_stream(stop_handler sh,tcp_client_ptr c):
	stream(p_user,sh),
	nsb(c,boost::bind(&mjpeg_stream::nsb_finalize,this,_1,_2),1000000),
	boundary_value("--ipcamera"){

		//delivery http-answer
		std::ostringstream oss;
		oss <<"HTTP/1.0 200 OK"<<std::endl;
		oss <<"Pragma: no-cache"<<std::endl;
		oss << "Server: some device"<<std::endl;
		oss << "Accept-Ranges: bytes"<<std::endl;
		oss << "Connection: close"<<std::endl;
		oss << "Content-Type: multipart/x-mixed-replace; boundary="<<boundary_value<<std::endl;
		oss << std::endl;

		try{
			c->get_socket().send(boost::asio::buffer(oss.str()));
		}
		catch(...){

		}
		

}

void mjpeg_stream::do_process_packet(packet_ptr p){
	//generate header
	std::ostringstream oss;
	oss << boundary_value;
	//std::string dt_str = boost::posix_time::to_simple_string(ts);
	//oss << "Date: "<<dt_str<<std::endl;
	//std::cout<<"frame formirated: "<<dt_str<<std::endl;
	oss << "Content-Length: "<<p->p.size<<std::endl;
	oss << std::endl;
	std::string header = oss.str();
	
	buffer_ptr bptr(new buffer());
	bptr->_data.reserve(header.size() + p->p.size);
	std::copy(header.begin(),header.end(),std::back_inserter(bptr->_data));				


	//send header
	//send jpeg
	nsb.add_buffer(bptr);
	nsb.add_buffer(buffer_ptr(new buffer(p)));

}

void mjpeg_stream::nsb_finalize(network_stream_buffer* _nsb, boost::system::error_code ec){
	call_stop_handler();
}


