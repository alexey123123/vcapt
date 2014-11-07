#ifndef __capturer_h__
#define __capturer_h__

#include <string>
#include <exception>
#include <deque>

#include <boost/shared_array.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/atomic.hpp>

#include <libswscale/swscale.h>

#include <system/Platform.h>
#include "types.h"
#include "libav.h"
//#include "FFMpeg_helper.h"


	struct __known_control__{
		std::string description;
		std::string system_name;
	};




	static struct __known_control__ known_controls[] = {
		{"Яркость",			"brightness"},
		{"Контрастность",	"contrast"},
		{"Насыщенность",		"saturation"},
		{"Цветовой тон",		"hue"},
		{"Гамма",			"gamma"},
		{"Освещенность",		"gain"},
		{"Резкость",			"sharpness"},
		{"Компенсация встречной засветки","backlight_compensation"},
		{"Фокус",			"focus_absolute"},
		{"Автофокус",		"focus_auto"},
		{"Увеличение",		"zoom_absolute"},


		{"",""}
	};

	struct CameraControl{
		std::string name;//brightness,hue,... etc (трансформируется в имя параметра)
		std::string description;//brightness,hue,... etc
		std::string current_value;
		std::string default_value;

	};

class capturer{
public:

	capturer():capturer_state(st_Initialization){};
	virtual ~capturer();




	//1. connect to device
	struct connect_parameters{
		//строка связи с камерой, прим:
		//	-	/dev/video(x) для v4l2 устройства
		//	-	<ip адрес>:<порт> для IP-камеры
		std::string connection_string;
		std::size_t maximum_buffer_size_mb;
		unsigned int		max_connect_attempts;
		unsigned int		connect_attempts_interval;
	};
	void connect(const connect_parameters& params);
	void disconnect();

	//2. Read device definition
	struct definition{
		std::string device_name;
		std::string manufacturer_name;
		std::string bus_info;
		std::string slot_name; //for v4l2 devices
	};
	definition get_definition() const
		{return do_get_definition();};

	struct format{
		AVPixelFormat ffmpeg_pixfmt;
		uint32_t v4l2_pixfmt;
		AVCodecID need_codec_id;

		enum type{
			fst_unknown,
			fst_discrete,
			fst_stepwise		
		};
		type type;
		std::deque<frame_size> framesizes;

		int width_min;
		int width_max;
		int width_step;
		int height_min;
		int height_max;
		int height_step;

		format();
		bool defined() const;
		bool check_framesize(const frame_size& fs) const;
		std::deque<frame_size> get_possible_framesizes() const;
	};

	format get_current_format() const
		{return do_get_current_format();};
	frame_size get_current_framesize()
		{return do_get_framesize();};
	void set_framesize(const frame_size& fsize);


	struct capabilities{

		enum _flags{
			f_start_stop_streaming	= 1 << 0
		};

		unsigned int flags;

		//поддерживаемые Controls и их текущие значения
		//std::deque<CameraControl> controls;

		capabilities();

		bool start_stop_streaming_supported() const
			{return (flags & f_start_stop_streaming) > 0;};
	};
	capabilities get_capabilities()
		{return do_get_capabilities();};

	void stop_streaming()
		{ do_stop_streaming();};
	void start_streaming()
		{ do_start_streaming();};

	enum state{
		st_Initialization,
		st_NotInitialized,
		st_InitializationError,
		st_ConnectError,	//for network cams
		st_CaptureError,
		st_Ready,
		st_Setup
	};
	state get_state() const ;



	frame_ptr get_frame(boost::chrono::steady_clock::time_point last_frame_tp);
	void return_frame(boost::chrono::steady_clock::time_point tp, void* opaque);

protected:





	virtual void do_connect(const connect_parameters& params) = 0;
	virtual void do_disconnect() = 0;
	//frame operations
	virtual frame_ptr do_get_frame(boost::chrono::steady_clock::time_point last_frame_tp) = 0;
	virtual void do_return_frame(boost::chrono::steady_clock::time_point tp, void* opaque){};

	virtual format do_get_current_format() const = 0;
	virtual void do_set_framesize(const frame_size& fsize) = 0;
	virtual frame_size do_get_framesize() = 0;
	virtual definition do_get_definition() const = 0;
	virtual capabilities do_get_capabilities() const = 0;

	//for cameras, which have internal streaming processes
	virtual void do_start_streaming() {};
	virtual void do_stop_streaming() {};

	virtual void on_state_change(state old_state,state new_state){};

	
private:
	
	AVFrame* convert_pix_fmt(AVFrame* src,AVPixelFormat dst_pix_fmt,std::string& error_message);

	


	boost::atomic<state> capturer_state;
	void set_state(state _state);

};



#endif//__capturer_h__
