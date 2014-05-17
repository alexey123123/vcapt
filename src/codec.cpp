#include "codec.h"
#include "codec_libav.h"
#include "codec_rpi.h"

codec::codec(){


}
codec::~codec(){
}



boost::shared_ptr<codec> codec::create(const format& f){

	//platform depended code
#if defined(RPI)
	//TODO: create OpenMax encoder (for H264 or JPEG)
	codec_ptr rpi_cptr = codec_ptr(new codec_rpi());
	rpi_cptr->initialize(f);
	return rpi_cptr;

#endif

	//create ffmpeg codec
	codec_ptr cptr = codec_ptr(new codec_libav());
	cptr->initialize(f);
	return cptr;
}

void codec::initialize(const format& f){
	do_initilalize(f,codec_pixfmt);
	_format = f;
}

packet_ptr codec::process_frame(capturer::frame_ptr fptr){

	//TODO: resizing, print text,...ect


	//check framesize & frame format

	capturer::frame_ptr fptr_to_encode = fptr;

	bool need_resize = false;

	//check framesize
	if ((_format.fsize.width != fptr->avframe->width)||(_format.fsize.height != fptr->avframe->height)){
		//TODO: if fsize change not enabled (due to CPU limitation)
		if (0)
			throw std::runtime_error("frame size change not enabled");

		frame_size fptr_fsize(fptr->avframe->width,fptr->avframe->height);
		
		if (_format.fsize > fptr_fsize)
			throw std::runtime_error("cannot increase frame size");

		need_resize = true;
	}

	if ((AVPixelFormat)fptr->avframe->format != codec_pixfmt){
		//TODO: if pixfmt convertation is not enabled (due to CPU limitation)
		if (0)
			throw std::runtime_error("frame size change not enabled");
		need_resize = true;
	}


	
	if (need_resize){
		//need rescale image
		std::string error_message;
		AVFrame* avframe_to_encode = 0;
		avframe_to_encode = resize_and_convert_format(fptr->avframe,_format.fsize.width,_format.fsize.height,codec_pixfmt,error_message);
		if (!avframe_to_encode)
			throw std::runtime_error("resize frame error");
		fptr_to_encode = capturer::frame_ptr(new capturer::frame());
		fptr_to_encode->avframe = avframe_to_encode;
		fptr_to_encode->tp = fptr->tp;

	}

	return do_process_frame(fptr_to_encode);
}








AVFrame* codec::resize_and_convert_format(AVFrame* src,int dst_width,int dst_height, AVPixelFormat dst_pixfmt,std::string& error_message){
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


codec::__resize_context::__resize_context(){
	c = 0;
	src_width = 0;
	src_height = 0;
	src_pix_fmt = AV_PIX_FMT_NONE;
	dst_width = 0;
	dst_height = 0;
	dst_pix_fmt = AV_PIX_FMT_NONE;

}
codec::__resize_context::~__resize_context(){
	if (c!=0)
		libav::sws_freeContext(c);
}

void codec::__resize_context::update(
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
