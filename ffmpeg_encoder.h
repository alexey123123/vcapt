#ifndef __encoder_h__
#define __encoder_h__


#include <queue>
#include <vector>

#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/chrono.hpp>


#include "types.h"
#include "capturer.h"
#include "libav.h"
#include "rpi_openmax.h"

class ffmpeg_encoder;


#define STREAM_BUFFER_SIZE 100000

class ffmpeg_encoder{
public:
	typedef boost::function<void (buffer_ptr)> data_handler;

	class hw_encoder_busy_exception{};

	ffmpeg_encoder(const std::string& _container_name, const std::string& _codec_name);
	~ffmpeg_encoder();

	std::deque<AVPixelFormat> get_pixel_formats();
	struct format{
		AVPixelFormat	pixfmt;	//input pixel format
		frame_size		fsize;	//input framesize
		int				bitrate;
	};
	void initialize(const format& f, data_handler _dh);


	void process_frame(capturer::frame_ptr fptr);

	int do_delivery_encoded_data(uint8_t *buf, int buf_size);

	buffer_ptr get_http_ok_answer();

	buffer_ptr get_header();


private:
	format _format;
	data_handler _data_handler;

	std::string container_name;
	AVCodecID codec_id;
	std::deque<AVPixelFormat> enc_formats;

	boost::chrono::steady_clock::time_point encoder_start_tp;

	bool do_encode_avframe(AVFrame* av_frame, std::string& error_message);

	AVCodec				*encoder_codec;
	AVFormatContext		*encoder_format_context;	
	AVStream			*encoder_video_stream;
	buffer_ptr			header_data_buffer;
	buffer_ptr			http_ok_answer_buffer;

	unsigned char out_stream_buffer[STREAM_BUFFER_SIZE];

	//rescale
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
	AVFrame* resize_and_convert_format(AVFrame* src,int dst_width,int dst_height, AVPixelFormat dst_pixfmt,std::string& error_message);


	void initial_data_handler(buffer_ptr);// for encoder construction

	rpi_openmax* rpi_hw_encoder;
};
typedef boost::shared_ptr<ffmpeg_encoder> EncoderPtr;

#endif//__encoder_h__