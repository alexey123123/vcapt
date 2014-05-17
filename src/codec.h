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
		std::string		name;
		AVPixelFormat	input_pixfmt;	//input pixel format (preffered format)
		frame_size		fsize;			//input framesize
		int				bitrate;
	};
	void initialize(const format& f);
	const format& get_format()
			{return _format;};

	
	
	static boost::shared_ptr<codec> create(const format& f);



	std::deque<AVPixelFormat> get_pixel_formats();

	packet_ptr process_frame(capturer::frame_ptr fptr);

	//need for ffmpeg based streams
	AVCodecContext* get_avcodec_context()
		{return do_get_avcodec_context();};

protected:

	virtual packet_ptr do_process_frame(capturer::frame_ptr fptr) = 0;
	virtual void do_initilalize(const format& f, AVPixelFormat& _codec_pixfmt) = 0;
	virtual AVCodecContext* do_get_avcodec_context() = 0;

	AVFrame* resize_and_convert_format(AVFrame* src,int dst_width,int dst_height, AVPixelFormat dst_pixfmt,std::string& error_message);
private:

	format _format;
	AVPixelFormat codec_pixfmt;//real codec pixfmt


	boost::chrono::steady_clock::time_point encoder_start_tp;

	struct __resize_context{
		int src_width;
		int src_height;
		AVPixelFormat src_pix_fmt;
		int dst_width;
		int dst_height;
		AVPixelFormat dst_pix_fmt;

		SwsContext* c;
		__resize_context();
		~__resize_context();

		void update(
			int _src_width,
			int _src_height,
			PixelFormat _src_pix_fmt,
			int _dst_width,
			int _dst_height,
			PixelFormat _dst_pix_fmt);

	} resize_context;
	
};
typedef boost::shared_ptr<codec> codec_ptr;


#endif//__codec_h__