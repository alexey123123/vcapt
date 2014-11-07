#include <boost/foreach.hpp>

#include "codec.h"
#include "codec_libav.h"
#include "codec_rpi.h"

codec::codec(){


}
codec::~codec(){
}



boost::shared_ptr<codec> codec::create(const format& f){

	//platform depended code
#if defined(OPENMAX_IL)	
	//TODO: create OpenMax encoder (for H264 or JPEG)
	if (f.codec_id == AV_CODEC_ID_H264){
		codec_ptr rpi_cptr = codec_ptr(new codec_rpi());
		rpi_cptr->initialize(f);
		return rpi_cptr;
	}

#endif

	//create libav codec
	codec_ptr cptr = codec_ptr(new codec_libav());
	cptr->initialize(f);
	return cptr;
}

void codec::initialize(const format& f){
	do_initilalize(f,codec_pixfmt);
	_format = f;
}

packet_ptr codec::process_frame(frame_ptr fptr){

	//check framesize & frame format

	frame_ptr fptr_to_encode = fptr;
	if (fptr_to_encode){
		bool need_resize = false;
		need_resize |= _format.fsize != fptr->get_framesize();
		need_resize |= (AVPixelFormat)fptr->avframe->format != codec_pixfmt;

		
		if (need_resize){
			std::string error_message;
			fptr_to_encode = f_helper.resize_and_convert(fptr_to_encode,codec_pixfmt,_format.fsize,error_message);
			if (!fptr_to_encode)
				throw std::runtime_error(error_message);
		}
	}



	return do_process_frame(fptr_to_encode);
}



/*
std::deque<AVPixelFormat> codec::get_pixel_formats(){
	//get supported format
	std::deque<AVPixelFormat> enc_formats;
	enc_formats.clear();
	const AVPixelFormat* pix_fmt = av_codec->pix_fmts;
	std::cout<<"Codec "<<libav::avcodec_get_name(av_codec->id)<<" supports pixfmts:";
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

	return enc_formats;
}
*/

void codec::get_header_packets(std::deque<packet_ptr>& packets){
	do_get_header_packets(packets);
	BOOST_FOREACH(packet_ptr p,packets)
		p->stream_header_data = true;
}