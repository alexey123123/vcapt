#ifndef __openmax_il_helper_h__
#define __openmax_il_helper_h__

#include <deque>
#include "libav.h"
#include "types.h"

class openmax_il_helper{
public:

	openmax_il_helper();
	~openmax_il_helper();


	//encoders
	void init_h264_encoder(int width,int height,int bitrate);
	void get_header_packets(std::deque<packet_ptr>& packets);
	bool encode_frame(AVFrame* input_frame,AVPacket& output_packet);
	//TODO:
	//bool init_jpeg_encoder(int width,int height);


	frame_ptr resize_frame(AVFrame* input_frame, AVPixelFormat dst_pixfmt, int dst_width, int dst_height);

private:
	class Impl;
	Impl* pimpl;

};

#endif//__openmax_il_helper_h__