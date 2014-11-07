#ifndef __libav_codec_h__
#define __libav_codec_h__

#include "libav.h"
#include "codec.h"

class codec_libav: public codec{
public:
	codec_libav();
	~codec_libav();

protected:
	virtual packet_ptr do_process_frame(frame_ptr fptr);
	virtual void do_initilalize(const format& f, AVPixelFormat& _codec_pixfmt);
	AVCodecContext* do_get_avcodec_context();
private:

	AVCodec* av_codec;
	AVCodecContext* av_codec_context;
	boost::chrono::steady_clock::time_point codec_start_tp;


};

#endif//__libav_codec_h__