#include "codec_libav.h"

codec_libav::codec_libav():av_codec(0),av_codec_context(0){
	codec_start_tp = boost::chrono::steady_clock::now();
}
codec_libav::~codec_libav(){
	if (av_codec_context != 0){
		libav::avcodec_close(av_codec_context);
		libav::av_free(av_codec_context);
	}

}
AVCodecContext* codec_libav::do_get_avcodec_context(){
	return av_codec_context;
}

packet_ptr codec_libav::do_process_frame(frame_ptr fptr){
	packet_ptr ret_packet(new packet());
	ret_packet->frame_tp = fptr->tp;

	using namespace boost::chrono;

	libav::av_init_packet(&ret_packet->p);
	ret_packet->p.data = 0;
	ret_packet->p.size = 0;
	ret_packet->p.pts = 0;
	ret_packet->p.dts = 0;
	ret_packet->p.duration = 0;
	//avframe_to_encode->quality = 1;

	AVFrame fr;
	memcpy(&fr,fptr->avframe,sizeof(AVFrame));
	fr.pts = boost::chrono::duration_cast<boost::chrono::milliseconds>(fptr->tp - codec_start_tp).count();

	int got_packet;
	libav::avcodec_encode_video2(av_codec_context, &ret_packet->p,&fr,&got_packet);
	if (!got_packet)
		ret_packet = packet_ptr();


	return ret_packet;

}

AVPixelFormat correct_deprecated_fmt(AVPixelFormat in_fmt){
	switch(in_fmt){
	case PIX_FMT_YUVJ420P:
		return PIX_FMT_YUV420P;
	case PIX_FMT_YUVJ422P:
		return PIX_FMT_YUV422P;
	case PIX_FMT_YUVJ444P:
		return PIX_FMT_YUV444P;
	}
	return in_fmt;
}


void codec_libav::do_initilalize(const format& f, AVPixelFormat& _codec_pixfmt){

	if (f.codec_id==AV_CODEC_ID_NONE)
		throw std::runtime_error("unknown codec");


	av_codec = libav::avcodec_find_encoder(f.codec_id);
	if (av_codec == 0)
		throw std::runtime_error("cannot find encoder");

	const AVPixelFormat* pix_fmt = av_codec->pix_fmts;
	const AVPixelFormat* selected_fmt = 0;
	while(pix_fmt != NULL){
		if (*pix_fmt == -1)
			break;

		//replace some deprecated formats
		AVPixelFormat fmt1 = correct_deprecated_fmt(*pix_fmt);

		if (fmt1==f.input_pixfmt){
			selected_fmt = pix_fmt;
			break;
		}
		pix_fmt++;
	}
	if (!selected_fmt){
		selected_fmt = av_codec->pix_fmts;//select first pixfmt from supported by codec
	}
		

	_codec_pixfmt = correct_deprecated_fmt(*selected_fmt);

	av_codec_context = libav::avcodec_alloc_context3(av_codec);
	if (!av_codec_context)
		throw std::runtime_error("avcodec_alloc_context3 error");

	av_codec_context->debug = 1;

	av_codec_context->codec_id = av_codec->id;
	av_codec_context->bit_rate = f.bitrate;
	/* Resolution must be a multiple of two. */
	av_codec_context->width    = f.fsize.width;
	av_codec_context->height   = f.fsize.height;
	av_codec_context->time_base.num = 1;
	av_codec_context->time_base.den = 1000;
	av_codec_context->gop_size = 11;
	av_codec_context->pix_fmt = *selected_fmt;



	//TODO: this flags depends from container........ 
	av_codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;

	AVCodecContext* ctx = av_codec_context;

	switch(av_codec->id){
	case AV_CODEC_ID_H264:
		av_codec_context->profile= FF_PROFILE_H264_BASELINE;
		av_codec_context->level = 41	;
		//libav::av_opt_set(av_codec_context->priv_data, "preset", "fast", 0);
		//libav::av_opt_set(av_codec_context->priv_data, "vprofile", "baseline", 0);

		/*

		ctx->bit_rate_tolerance = 0;
		ctx->rc_max_rate = 0;
		ctx->rc_buffer_size = 0;
		ctx->gop_size = 40;
		ctx->max_b_frames = 3;
		ctx->b_frame_strategy = 1;
		ctx->coder_type = 1;
		ctx->me_cmp = 1;
		ctx->me_range = 16;
		ctx->qmin = 10;
		ctx->qmax = 51;
		ctx->scenechange_threshold = 40;
		ctx->flags |= CODEC_FLAG_LOOP_FILTER;
		ctx->me_method = ME_HEX;
		ctx->me_subpel_quality = 5;
		ctx->i_quant_factor = 0.71;
		ctx->qcompress = 0.6;
		ctx->max_qdiff = 4;
		//ctx->directpred = 1;
		//ctx->flags2 |= CODEC_FLAG2_FASTPSKIP;
		*/

		break;
	case AV_CODEC_ID_VP8:
	case AV_CODEC_ID_VP9:
		libav::av_opt_set(av_codec_context->priv_data, "quality", "realtime", 0);
		break;
	case AV_CODEC_ID_MJPEG:
		av_codec_context->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
		av_codec_context->flags |= CODEC_FLAG_QSCALE;
		break;
	}

	if (libav::avcodec_open2(av_codec_context, av_codec,0) < 0)
		throw std::runtime_error("avcodec_open2 error");

	std::cout<<"codec_libav::do_initilalize ok"<<std::endl;
}
