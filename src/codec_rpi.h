#ifndef __codec_rpi_h__
#define __codec_rpi_h__


#include "codec_libav.h"

//hardware encoder. used RPI OpenMax

class codec_rpi: public codec_libav{
public:
	codec_rpi();
	~codec_rpi();

protected:
	packet_ptr do_process_frame(capturer::frame_ptr fptr);
	void do_initilalize(const format& f, AVPixelFormat& _codec_pixfmt);
private:

	class Impl;
	Impl* pimpl;

	boost::chrono::steady_clock::time_point codec_start_tp;

	void init_h264(const format& f);

};

#endif//__codec_rpi_h__