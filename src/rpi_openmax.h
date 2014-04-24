#ifndef __RPi_OpenMax_h__
#define __RPi_OpenMax_h__

#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>



//hradware RPI H264 encoder

class rpi_openmax{
public:
	rpi_openmax();
	~rpi_openmax();

	bool initialize(int width,int height,int bitrate);

	bool encode_frame(AVFrame* input_frame,AVPacket& output_packet);
private:
	class Impl;
	Impl* pimpl;
};

#endif//__RPi_OpenMax_h__