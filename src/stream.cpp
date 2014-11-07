#include <boost/asio/write.hpp>
#include <boost/bind.hpp>
#include <boost/asio/placeholders.hpp>

#include <utility/Journal.h>

#include "stream.h"

stream::stream(priority p, stop_handler sh):_priority(p), _stop_handler(sh),
	stream_start_ptime(boost::posix_time::not_a_date_time),
_stopping(false),bytes_written_at(0){
};

stream::~stream(){
}

void stream::call_stop_handler(const std::string& reason){
	if (reason != ""){
		using namespace Utility;
		Journal::Instance()->Write(INFO,DST_STDOUT|DST_SYSLOG,"stream stopped:%s",reason.c_str());
	}

	_stopping = true;
	_stop_handler(this);
}

bool stream::stopping() const {
	return _stopping;
}

void stream::start(){
	stream_start_tp = boost::chrono::steady_clock::now();
	stream_start_ptime = boost::posix_time::second_clock::local_time();
}

bool stream::started() const{
	return stream_start_ptime != boost::posix_time::not_a_date_time;
}

int stream::get_duration_secs() const{
	typedef boost::chrono::duration<long, boost::ratio<1> > seconds;
	seconds m = boost::chrono::duration_cast<seconds>(boost::chrono::steady_clock::now() - stream_start_tp);
	return m.count();
}
boost::int64_t stream::get_written_bytes() const{
	return bytes_written_at;
}



av_container_stream::av_container_stream(priority p, stop_handler sh,tcp_client_ptr c):stream(p,sh),
	format_context(0),video_stream(0),current_delay(-1),
	//nsb(c,boost::bind(&av_container_stream::nsb_finalize,this,_1,_2), 10000000),
	kf_found(false),_tcp_client(c)
{
	
}

av_container_stream::av_container_stream(stop_handler sh):stream(p_save_to_file,sh),
	format_context(0),video_stream(0),	
	kf_found(false),
	first_error_frame_tp(boost::chrono::steady_clock::time_point::max())
{

}

av_container_stream::~av_container_stream(){

	

	if (format_context){

		//TODO: write_trailer
		if (its_a_file_stream()){
			libav::av_write_trailer(format_context);
			file_stream.close();
			std::cout<<"trailer written, file closed"<<std::endl;
		}

		if (format_context->pb != 0){
			libav::av_free(format_context->pb);
			format_context->pb = 0;
		}
		

		if (video_stream!= 0){
			if (video_stream->codec != 0)
				libav::avcodec_close(video_stream->codec);
		}

		libav::avformat_free_context(format_context);

	}

	//nsb.close();



}

void av_container_stream::nsb_finalize(network_stream_buffer* _nsb, boost::system::error_code ec){
	call_stop_handler("delivery data error");
}



int __write_packet__2(void *opaque, uint8_t *buf, int buf_size);
int64_t __seek__(void *opaque, int64_t offset, int whence);


stream_ptr av_container_stream::create_network_stream(const client_parameters& c_params, 
														AVCodecContext* av_codec_context, 
														stream::stop_handler sh,
														tcp_client_ptr c){

	if (c_params.container_name=="mjpeg"){
		return boost::shared_ptr<mjpeg_stream>(new mjpeg_stream(sh,c));
	}

	av_container_stream* avs(new av_container_stream(
									c->internal_client ? stream::p_system_service : stream::p_user,
									sh,c)); 


		try{
			avs->alloc_stream(c_params.container_name,av_codec_context);


			//network stream stuff
			avs->format_context->pb = libav::avio_alloc_context(avs->out_stream_buffer,STREAM_BUFFER_SIZE,1,0,0,__write_packet__2,0);
			if (!avs->format_context->pb)
				throw std::runtime_error("avio_alloc_context error");
			avs->format_context->pb->opaque = (void*)avs;

			avs->autostop_timeout = c_params.autostop_timeout_sec;

			//generate http-answer
			std::string http_ok_answer = avs->get_http_ok_answer(c_params.container_name);
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

stream_ptr av_container_stream::create_file_stream(
	filestream_params fstream_params, 
	AVCodecContext* av_codec_context, 
	stream::stop_handler sh,
	progress_handler ph,
	const boost::filesystem::path& p
	){


		av_container_stream* avs(new av_container_stream(sh)); 
		avs->file_path = p;
		avs->_progress_handler = ph;
		avs->fs_params = fstream_params;

		avs->autostop_timeout = boost::optional<int>(5000);


		try{

			avs->alloc_stream(avs->fs_params.c_params.container_name,av_codec_context);


			
			avs->format_context->pb = libav::avio_alloc_context(avs->out_stream_buffer,STREAM_BUFFER_SIZE,1,0,0,__write_packet__2,__seek__);
			if (!avs->format_context->pb)
				throw std::runtime_error("avio_alloc_context error");
			avs->format_context->pb->opaque = (void*)avs;

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

	video_stream->id = format_context->nb_streams-1;
	video_stream->index = format_context->nb_streams-1;

	AVCodecContext* c = video_stream->codec;

	c->codec_id = av_codec_context->codec_id;
	c->debug = 1;
	c->bit_rate = av_codec_context->bit_rate;
	c->width    = av_codec_context->width;
	c->height   = av_codec_context->height;
	c->time_base = av_codec_context->time_base;
	c->gop_size = av_codec_context->gop_size;
	c->pix_fmt = av_codec_context->pix_fmt;
	c->flags = av_codec_context->flags;
	c->profile = av_codec_context->profile;

	if (libav::avcodec_open2(c, av_codec_context->codec,0) < 0)
		throw std::runtime_error("avcodec_open2 error");
}



void av_container_stream::do_process_packet(packet_ptr p){
	
	if (!p->stream_header_data)
		if (!started())
			return ;

	if (stopping())
		return ;


	//check autostop
	if (autostop_timeout){
		if (p->packet_flags & PACKET_FLAG_STREAM_ERROR_BIT){
			if (first_error_frame_tp){
				//check duration

				boost::chrono::steady_clock::time_point now_tp = boost::chrono::steady_clock::now();
				typedef boost::chrono::duration<long,boost::milli> milliseconds;
				milliseconds ms = boost::chrono::duration_cast<milliseconds>(now_tp - (*first_error_frame_tp));
				if (ms.count() > (*autostop_timeout)*1000){
					std::cout<<"av_container_stream: autostop!"<<std::endl;
					call_stop_handler("stream settings error");
				}
				
					
			} else{
				first_error_frame_tp = TP(boost::chrono::steady_clock::now());
				using namespace Utility;
				Journal::Instance()->Write(NOTICE,DST_STDOUT|DST_SYSLOG,"Stream parameter changed. Stop in %d seconds",(*autostop_timeout));
			}
				

		} else{
			//reset first_error_frame_tp
			if (first_error_frame_tp){
				first_error_frame_tp = TP();
				using namespace Utility;
				Journal::Instance()->Write(NOTICE,DST_STDOUT|DST_SYSLOG,"Stream parameters normalized");
			}
			
		}

	}



	AVPacket* avpacket = &(p->p);

	//wait keyframe
	if (!kf_found){
		kf_found = (avpacket->flags & AV_PKT_FLAG_KEY) > 0;
  		if (!kf_found)
  			return ;
	}
		

	//TODO: control start_ptime
	
	//write packet to container


	avpacket->stream_index = video_stream->id;


	avpacket->pts = 1;
	if (!p->stream_header_data){
		avpacket->pts = boost::chrono::duration_cast<boost::chrono::milliseconds>(p->frame_tp - stream_start_tp).count();
		boost::chrono::steady_clock::time_point now_tp = boost::chrono::steady_clock::now();
		int delay_s = boost::chrono::duration_cast<boost::chrono::seconds>(now_tp - p->frame_tp).count();
		if (current_delay != delay_s){
			current_delay = delay_s;
			std::cout<<"current delay: "<<current_delay<<" seconds"<<std::endl;
		}
	}
		

	//control max_duration
	if (its_a_file_stream())
		if (fs_params.max_duration_mins)
			if (*fs_params.max_duration_mins > 0)
				if (avpacket->pts > (*fs_params.max_duration_mins) * 60 * 1000){
					using namespace Utility;
					Journal::Instance()->Write(NOTICE,DST_SYSLOG|DST_STDOUT,"filestream stopped. max duration exceeded");
					call_stop_handler();
					return ;
				}



	AVRational r1;
	r1.num = 1;
	r1.den = 1000;
	avpacket->pts = libav::av_rescale_q_rnd(avpacket->pts, r1, video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
	avpacket->dts = avpacket->pts;
	//avpacket->duration = libav::av_rescale_q(avpacket->duration, r1, video_stream->time_base);
	avpacket->duration = 0;	

	//transfer packet to stream
	libav::av_write_frame(format_context,avpacket);


}


#define NB_SIZE 1*1024*1024

network_stream_buffer::network_stream_buffer(tcp_client_ptr _tcptr,stop_handler _h, unsigned int max_buffer_size):
tcptr(_tcptr),_stop_handler(_h),_max_buffer_size(max_buffer_size),
total_size(0),sent_packets_count(0),
last_debug_output_pt(boost::posix_time::min_date_time),
total_delivery_time_ms(0),
deliveries_count(0){

}
network_stream_buffer::~network_stream_buffer(){
	close();
}
void network_stream_buffer::close(){
	tcptr = tcp_client_ptr();
}

void network_stream_buffer::add_buffer(buffer_ptr b){
	if (client_error_code)
		return ;

	try{
		boost::system::error_code ec;
		boost::asio::write(tcptr->get_socket(),
			boost::asio::buffer(b->data(),b->size()),ec);
		if (ec){
			client_error_code = ec;
			//_stop_handler()
		}
	}
	catch(...){
		throw std::runtime_error("delivery error");
	}

	return;

	boost::unique_lock<boost::recursive_mutex> l1(buffers_mutex);




	/*
	boost::asio::async_write(tcptr->get_socket(),
		boost::asio::buffer(b->data(),b->size()),
		boost::bind(&network_stream_buffer::write_data_handler, 
		this,
		b,
		boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
	*/
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
			current_buffer->pt = boost::posix_time::microsec_clock::universal_time();
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
		//std::cout<<"bytes_transferred:"<<bytes_transferred<<std::endl;
		if (bptr->size() != bytes_transferred)
			std::cout<<"------------TRANSFERED only "<<bytes_transferred<<" bytes from "<<bptr->size()<<std::endl;

		//send next data buffer

		if (current_buffer){
			total_size -= current_buffer->size();
			current_buffer = buffer_ptr();
		}


		boost::posix_time::ptime now_pt = boost::posix_time::microsec_clock::universal_time();
		boost::posix_time::time_duration td = now_pt - last_debug_output_pt;
		//std::cout<<td.total_seconds()<<std::endl;
		if ((td.total_seconds() > 5)||(last_debug_output_pt==boost::posix_time::min_date_time)){
			std::ostringstream oss;
			oss<<"NSB debug info: queue_size:"<<buffers.size()<<",cp_count:"<<sent_packets_count;
			if (deliveries_count>0){
				unsigned int avg_d_time = total_delivery_time_ms / deliveries_count;
				oss<<",avg delivery time:"<<avg_d_time<<" ms ("<<deliveries_count<<" packets)";
				total_delivery_time_ms = 0;
				deliveries_count = 0;
			}


			std::cout<<oss.str()<<std::endl;
			last_debug_output_pt = now_pt;
		}

		//delivery time
		td = now_pt - bptr->pt;
		total_delivery_time_ms += td.total_milliseconds();
		deliveries_count++;


		send_first_buffer();

	} else{
		//deleting client
		std::string e_mess = ec.message();
		//std::cout<<"delivery error (bt:"<<bytes_transferred<<")("<<e_mess<<")"<<std::endl;
		client_error_code = ec;


		//client disconnected
		_stop_handler(this,client_error_code);
	}

}


int av_container_stream::do_write(uint8_t *buf, int buf_size){

	if (buf_size==0)
		return 0;

	if (its_a_network_stream()){

		//write directly to socket

		boost::system::error_code ec;
		boost::asio::write(_tcp_client->get_socket(),boost::asio::buffer(buf,buf_size),ec);
		if (ec){
			//delivery error
			call_stop_handler("delivery data error");
		}

		/*
		buffer_ptr b(new buffer());
		b->_data.reserve(buf_size);
		std::copy(buf,buf+buf_size,std::back_inserter(b->_data));
		nsb.add_buffer(b);
		*/
	}

	boost::int64_t bw = bytes_written_at;

	if (its_a_file_stream()){

		

		if (fs_params.max_filesize_mb)
			if (bw + buf_size >= (*fs_params.max_filesize_mb)*1024*1024){
				using namespace Utility;
				Journal::Instance()->Write(NOTICE,DST_SYSLOG|DST_STDOUT,"filestream stopped. max filesize exceeded");
				call_stop_handler();

				return 0;
			}


		//file stream stuff
		if (!file_stream.is_open()){
			file_stream.open(file_path.string().c_str(),std::ios::binary|std::ios::out);
			if (!file_stream){
				using namespace Utility;
				Journal::Instance()->Write(NOTICE,DST_SYSLOG|DST_STDOUT,"filestream stopped. cannot create file");
				call_stop_handler();

				return 0;

			}
		}


		file_stream.write((char*)buf,buf_size);
		if (!file_stream){
			using namespace Utility;
			Journal::Instance()->Write(NOTICE,DST_SYSLOG|DST_STDOUT,"filestream stopped. write data error");
			call_stop_handler();

			return 0;
		}
		

		

			
	}

	bw += buf_size;
	bytes_written_at.exchange(bw,boost::memory_order_release);

	if (its_a_file_stream()){
		//TODO: call progress handler
	}


	return buf_size;
}

int64_t av_container_stream::do_seek(int64_t offset, int whence){
	//std::cout<<"do_seek (off:"<<offset<<")!"<<std::endl;
	if (its_a_file_stream())
		if (file_stream){
			int64_t old_pos = file_stream.tellp();
			file_stream.seekp(offset);
		}
	return offset;
}



int __write_packet__2(void *opaque, uint8_t *buf, int buf_size){
	av_container_stream* s = (av_container_stream*)opaque;
	return s->do_write(buf,buf_size);
}

int64_t __seek__(void *opaque, int64_t offset, int whence){
	av_container_stream* s = (av_container_stream*)opaque;
	return s->do_seek(offset,whence);
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
	call_stop_handler("delivery data error");
}


