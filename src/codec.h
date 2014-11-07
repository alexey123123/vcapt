#ifndef __codec_h__
#define __codec_h__

#include "libav.h"
#include "types.h"
#include "capturer.h"

class codec{
public:

	class ex_codecs_limit_overflow{};

	codec();
	~codec();

	struct format{
		AVCodecID		codec_id;
		AVPixelFormat	input_pixfmt;	//input pixel format (preffered format)
		frame_size		fsize;			//input framesize
		int				bitrate;
	};
	void initialize(const format& f);
	const format& get_format()
			{return _format;};

		
	static boost::shared_ptr<codec> create(const format& f);

	packet_ptr process_frame(frame_ptr fptr);

	//need for ffmpeg based streams
	AVCodecContext* get_avcodec_context()
		{return do_get_avcodec_context();};

	//some codecs may contain header data inside (codec_rpi)
	void get_header_packets(std::deque<packet_ptr>& packets);
		
		

protected:

	virtual packet_ptr do_process_frame(frame_ptr fptr) = 0;
	virtual void do_initilalize(const format& f, AVPixelFormat& _codec_pixfmt) = 0;
	virtual AVCodecContext* do_get_avcodec_context() = 0;
	virtual void do_get_header_packets(std::deque<packet_ptr>& packets){};

	
private:

	format _format;
	AVPixelFormat codec_pixfmt;//real codec pixfmt


	boost::chrono::steady_clock::time_point encoder_start_tp;
	frame_helper f_helper;


	
};
typedef boost::shared_ptr<codec> codec_ptr;


#endif//__codec_h__