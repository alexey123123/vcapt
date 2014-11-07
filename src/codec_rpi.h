#ifndef __codec_rpi_h__
#define __codec_rpi_h__


#include "codec_libav.h"
#include "openmax_il_helper.h"

//hardware encoder. used RPI OpenMax

class codec_rpi: public codec_libav{
public:
	codec_rpi();
	~codec_rpi();

protected:
	packet_ptr do_process_frame(frame_ptr fptr);
	void do_initilalize(const format& f, AVPixelFormat& _codec_pixfmt);

	void do_get_header_packets(std::deque<packet_ptr>& packets);
private:

	openmax_il_helper omx_helper;
	openmax_il_helper omx_helper_2;


	class Impl;
	Impl* pimpl;

	boost::chrono::steady_clock::time_point codec_start_tp;


};

#endif//__codec_rpi_h__