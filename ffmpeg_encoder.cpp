#include <iostream>
#include <fstream>

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/time_formatters_limited.hpp>
#include <boost/foreach.hpp>

#include <libavcodec/avcodec.h>

#include <utility/Library.h>

#include "ffmpeg_encoder.h"
#include "libav.h"

#include "rpi_openmax.h"

static rpi_openmax* rpi_openmax_encoder = 0;


#define MJPEG_INITIAL_FRAME_INTERVAL_MS 1000
#define MAX_CLIENT_BUFFER_SIZE 1000000



buffer_ptr generate_mjpeg_part_header2(std::vector<uchar>::size_type data_size,boost::posix_time::ptime ts);


int __write_packet__(void *opaque, uint8_t *buf, int buf_size);
int64_t __seek__(void *opaque, int64_t offset, int whence);





ffmpeg_encoder::ffmpeg_encoder(const std::string& _container_name, const std::string& _codec_name):
			container_name(_container_name),


	encoder_video_stream(0),
	encoder_format_context(0),
			rpi_hw_encoder(0){


		libav::avformat_alloc_output_context2(&encoder_format_context, NULL, container_name.c_str(), NULL);
		if (!encoder_format_context)
			throw std::runtime_error("cannot initialize container ("+container_name+")");

		codec_id = AV_CODEC_ID_NONE;
		if (_codec_name=="h264")
			codec_id = AV_CODEC_ID_H264;
		if (_codec_name=="vp8")
			codec_id = AV_CODEC_ID_VP8;
		if (_codec_name=="vp9")
			codec_id = AV_CODEC_ID_VP9;
		if (_codec_name=="mjpeg")
			codec_id = AV_CODEC_ID_MJPEG;

		if (codec_id==AV_CODEC_ID_NONE)
			throw std::runtime_error("unknown codec ("+_codec_name+")");


#if defined(RPI)
		if (codec_id==AV_CODEC_ID_H264)
			rpi_hw_encoder = new rpi_openmax();
#endif

		encoder_codec = libav::avcodec_find_encoder(codec_id);
		if (encoder_codec == 0)
			throw std::runtime_error("cannot find encoder ("+_codec_name+")");

		encoder_video_stream = libav::avformat_new_stream(encoder_format_context, encoder_codec);
		if (!encoder_video_stream)
			throw std::runtime_error("cannot create stream (codec incompatible with container?)");

		encoder_video_stream->id = encoder_format_context->nb_streams-1;
		encoder_video_stream->index = encoder_format_context->nb_streams-1;

		//get supported format
		enc_formats.clear();
		const AVPixelFormat* pix_fmt = encoder_codec->pix_fmts;
		std::cout<<"Codec "<<libav::avcodec_get_name(codec_id)<<" supports pixfmts:";
		while(pix_fmt != NULL){
			if (*pix_fmt == -1)
				break;

			//replace some deprecated formats
			AVPixelFormat pfmt_to_add;
			switch(*pix_fmt){
			case PIX_FMT_YUVJ420P:
				pfmt_to_add = PIX_FMT_YUV420P;
				break;
			case PIX_FMT_YUVJ422P:
				pfmt_to_add = PIX_FMT_YUV422P;
				break;
			case PIX_FMT_YUVJ444P:
				pfmt_to_add = PIX_FMT_YUV444P;
				break;
			default:
				pfmt_to_add = *pix_fmt;
				break;
			}




			enc_formats.push_back(pfmt_to_add);

			std::cout<<libav::av_get_pix_fmt_name(pfmt_to_add)<<" ";

			pix_fmt++;
		}
		std::cout<<std::endl;

}



ffmpeg_encoder::~ffmpeg_encoder(){

	boost::system::error_code ec;

	if (encoder_format_context != 0){
		if (encoder_video_stream != 0)
			if (encoder_video_stream->codec != 0)
				libav::avcodec_close(encoder_video_stream->codec);

		if (encoder_format_context->pb != 0){
			libav::av_free(encoder_format_context->pb);
			encoder_format_context->pb = 0;
		}

		libav::avformat_free_context(encoder_format_context);
	}

	if (rpi_hw_encoder)
		delete (rpi_hw_encoder);

	std::cout<<"encoder ("<<libav::avcodec_get_name(codec_id)<<") finished"<<std::endl;

}


std::deque<AVPixelFormat> ffmpeg_encoder::get_pixel_formats(){
	return enc_formats;
}
void ffmpeg_encoder::initialize(const format& f, data_handler _dh){
	_format = f;
	_data_handler = _dh;

			AVCodecContext *c = encoder_video_stream->codec;

			c->codec_id = encoder_codec->id;
			c->bit_rate = _format.bitrate;
			/* Resolution must be a multiple of two. */
			c->width    = _format.fsize.width;
			c->height   = _format.fsize.height;
			c->time_base.num = 1;
			c->time_base.den = 1000;
			c->gop_size = 11;
			c->pix_fmt = _format.pixfmt;

			if (encoder_format_context->oformat->flags & AVFMT_GLOBALHEADER)
				c->flags |= CODEC_FLAG_GLOBAL_HEADER;
			

			switch(codec_id){
				case AV_CODEC_ID_H264:
					c->profile= FF_PROFILE_H264_BASELINE;
					libav::av_opt_set(c->priv_data, "preset", "fast", 0);
					break;
				case AV_CODEC_ID_VP8:
				case AV_CODEC_ID_VP9:
					libav::av_opt_set(c->priv_data, "quality", "realtime", 0);
					break;
				case AV_CODEC_ID_MJPEG:
					c->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
					c->flags |= CODEC_FLAG_QSCALE;
					break;
			}




			if (libav::avcodec_open2(c, encoder_codec,0) < 0)
				throw std::runtime_error("avcodec_open2 error");

			
			//encoder_format_context->pb = libav::avio_alloc_context(out_stream_buffer,WEBM_STREAM_BUFFER_SIZE,1,0,0,__write_packet__,__seek__);

			
			encoder_format_context->pb = libav::avio_alloc_context(out_stream_buffer,STREAM_BUFFER_SIZE,1,0,0,__write_packet__,__seek__);
			if (!encoder_format_context->pb)
				throw std::runtime_error("avio_alloc_context error");
			encoder_format_context->pb->opaque = (void*)this;

			/*			
			
			int ret = libav::avio_open(&encoder_format_context->pb, "/var/file2.flv", AVIO_FLAG_WRITE);
			if (ret < 0)
				throw std::runtime_error("avio_open error");
			*/
			
			
			if (libav::avformat_write_header(encoder_format_context,0) != 0)
				throw std::runtime_error("stream error (avformat_write_header < 0)");

			encoder_start_tp = boost::chrono::steady_clock::now();

			if (rpi_hw_encoder)
				rpi_hw_encoder->initialize(f.fsize.width,f.fsize.height,f.bitrate);
}


int __write_packet__(void *opaque, uint8_t *buf, int buf_size){
	ffmpeg_encoder* enc = (ffmpeg_encoder*)opaque;
	return enc->do_delivery_encoded_data(buf,buf_size);
}

int64_t __seek__(void *opaque, int64_t offset, int whence){
	//printf("__webm_seek__(offset:%d,whence:%d)\n",offset,whence);
	return offset;
}

void ffmpeg_encoder::process_frame(capturer::frame_ptr fptr){
	if (fptr){

		using namespace boost::chrono;




		//TODO: resizing, print text,...ect


		//check framesize & frame format
		boost::chrono::steady_clock::time_point fptr_tp = fptr->tp;

		AVFrame* avframe_to_encode = 0;
		AVFrame fr1;
		bool need_resize = false;
		need_resize |= ((_format.fsize.width != fptr->avframe->width)||(_format.fsize.height != fptr->avframe->height));
		need_resize |= (AVPixelFormat)fptr->avframe->format != _format.pixfmt;
		if (need_resize){
			//need rescale image
			std::string error_message;
			avframe_to_encode = resize_and_convert_format(fptr->avframe,_format.fsize.width,_format.fsize.height,_format.pixfmt,error_message);
			if (!avframe_to_encode)
				throw std::runtime_error("resize frame error");
			fptr = capturer::frame_ptr();//release capturer frame
		} else{
			//dublicate fptr->avframe, because need to change avframe_to_encode->pts
			fr1 = *(fptr->avframe);
			avframe_to_encode = &fr1;
		}

		avframe_to_encode->pts = boost::chrono::duration_cast<boost::chrono::milliseconds>(fptr_tp - encoder_start_tp).count();

		std::string error_message;
		if (!do_encode_avframe(avframe_to_encode,error_message))
			throw std::runtime_error("encode error:"+error_message);


		if (avframe_to_encode != &fr1){
			//need delete avframe_to_encode
			libav::av_frame_free(&avframe_to_encode);
		}



	}
}




bool ffmpeg_encoder::do_encode_avframe(AVFrame* av_frame, std::string& error_message){

	using namespace boost::chrono;

	try{

		AVPacket packet;
		libav::av_init_packet(&packet);
		packet.data = 0;
		packet.size = 0;
		av_frame->quality = 1;

		int got_packet;
		AVCodecContext *c = encoder_video_stream->codec;
		
		switch(codec_id){
			case AV_CODEC_ID_MJPEG:{
				libav::avcodec_encode_video2(c, &packet,av_frame,&got_packet);
				buffer_ptr part_header_fptr = generate_mjpeg_part_header2(packet.size,boost::posix_time::microsec_clock::universal_time());
				_data_handler(part_header_fptr);

				//directly call (without stream)
				do_delivery_encoded_data(packet.data,packet.size);


				break;
			}
			default:{
				packet.pts = 0;
				got_packet = 0;			
				if (!rpi_hw_encoder)
					libav::avcodec_encode_video2(c, &packet,av_frame,&got_packet); 
				else{
					rpi_hw_encoder->encode_frame(av_frame,packet);
					got_packet = 1;
				}
					


				if (got_packet){

					packet.stream_index = encoder_video_stream->id;

					AVRational r1;
					r1.num = 1;
					r1.den = 1000;
					if (packet.pts==0){
						packet.pts = libav::av_rescale_q_rnd(av_frame->pts, r1, encoder_video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
					}
					packet.dts = packet.pts;
					packet.duration = (int)libav::av_rescale_q(packet.duration, r1, encoder_video_stream->time_base);
					

					//transfer packet to stream
					libav::av_write_frame(encoder_format_context,&packet);
				}

			}
		}

		libav::av_free_packet(&packet);

		return true;
	}
	catch(std::runtime_error* ex){
		error_message = std::string(ex->what());
	}
	return false;

}




ffmpeg_encoder::__resize_context::__resize_context(){
	c = 0;
	src_width = 0;
	src_height = 0;
	src_pix_fmt = AV_PIX_FMT_NONE;
	dst_width = 0;
	dst_height = 0;
	dst_pix_fmt = AV_PIX_FMT_NONE;

}
ffmpeg_encoder::__resize_context::~__resize_context(){
	if (c!=0)
		libav::sws_freeContext(c);
}


void ffmpeg_encoder::__resize_context::update(
	int _src_width,
	int _src_height,
	PixelFormat _src_pix_fmt,
	int _dst_width,
	int _dst_height,
	PixelFormat _dst_pix_fmt){

		if ((_src_width==src_width) &&
			(_src_height == src_height) &&
			(_src_pix_fmt == src_pix_fmt) &&
			(_dst_width == dst_width) &&
			(_dst_height == dst_height) &&
			(_dst_pix_fmt == dst_pix_fmt))
			return ;

		if (c!=0)
			libav::sws_freeContext(c);
		
		c =  libav::sws_getContext( _src_width, _src_height, 
			_src_pix_fmt,
			_dst_width, _dst_height, 
			_dst_pix_fmt, SWS_BICUBIC, NULL, NULL, NULL );
		if (!c)
			throw std::runtime_error("cannot allocate SwsContext");

}



AVFrame* ffmpeg_encoder::resize_and_convert_format(AVFrame* src,int dst_width,int dst_height, AVPixelFormat dst_pixfmt,std::string& error_message){
	AVFrame* dst = 0;
	int res=-55;
	try{
		resize_context.update(src->width,src->height,(PixelFormat)src->format,dst_width,dst_height,dst_pixfmt);
		

		dst = libav::av_frame_alloc();
		dst->width = dst_width;
 		dst->height = dst_height;
		dst->format = dst_pixfmt;
		libav::av_frame_get_buffer(dst,1);

		res = libav::sws_scale( resize_context.c ,
			src->data, src->linesize, 
			0, 
			src->height,
			dst->data, dst->linesize ); 
		dst->pts = src->pts;
	}
	catch(std::runtime_error& ex){
		if (dst != 0)
			libav::av_frame_free(&dst);
		error_message = std::string(ex.what());
	}





	return dst;

}











int ffmpeg_encoder::do_delivery_encoded_data(uint8_t *buf, int buf_size){
	//std::cout<<"Encoder::webm_write_packet("<<buf_size<<" bytes)"<<std::endl;
	if (buf_size==0)
		return 0;

	buffer_ptr b(new buffer());
	b->_data.reserve(buf_size);
	std::copy(buf,buf+buf_size,std::back_inserter(b->_data));

	//need remember first data (header of stream)
	switch(codec_id){
		case AV_CODEC_ID_MJPEG:
			break;
		default:
			if (!header_data_buffer){
				std::cout<<"-------header remembed"<<std::endl;
				header_data_buffer = b;

				return buf_size;
			}
			break;
	}
	
	_data_handler(b);

	return buf_size;
}












buffer_ptr ffmpeg_encoder::get_http_ok_answer(){
	if (!http_ok_answer_buffer){
		std::ostringstream oss;
		oss <<"HTTP/1.0 200 OK"<<std::endl;
		oss <<"Pragma: no-cache"<<std::endl;

		//content-type from container
		switch(codec_id){
			case AV_CODEC_ID_MJPEG:
				oss << "Server: some device"<<std::endl;
				oss << "Accept-Ranges: bytes"<<std::endl;
				oss << "Connection: close"<<std::endl;
				oss << "Content-Type: multipart/x-mixed-replace; boundary=--ipcamera"<<std::endl;
				break;
			default:
				oss <<"Content-Type: video/"<<container_name<<std::endl;
				break;
		}
		oss << std::endl;
		std::string a_string = oss.str();


		http_ok_answer_buffer = buffer_ptr(new buffer());
		http_ok_answer_buffer->_data.reserve(a_string.size());
		std::copy(a_string.begin(),a_string.end(),std::back_inserter(http_ok_answer_buffer->_data));

	}
	return http_ok_answer_buffer;
}


buffer_ptr ffmpeg_encoder::get_header(){
	return header_data_buffer;
}

buffer_ptr generate_mjpeg_part_header2(std::vector<uchar>::size_type data_size,boost::posix_time::ptime ts){
	std::ostringstream oss;
	oss << "--ipcamera"<<std::endl;
	std::string dt_str = boost::posix_time::to_simple_string(ts);
	oss << "Date: "<<dt_str<<std::endl;
	//std::cout<<"frame formirated: "<<dt_str<<std::endl;
	oss << "Content-Length: "<<data_size<<std::endl;
	oss << std::endl;
	std::string mjpeg_http_header = oss.str();
	buffer_ptr bptr(new buffer());
	bptr->_data.reserve(mjpeg_http_header.size() + data_size);
	std::copy(mjpeg_http_header.begin(),
		mjpeg_http_header.end(),
		std::back_inserter(bptr->_data));				
	return bptr;

}


