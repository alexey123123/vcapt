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
		{return DoGetDefinition();};

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
		{return DoGetCurrentFormat();};
	frame_size get_current_framesize()
		{return DoGetFramesize();};



	void set_framesize(const frame_size& fsize);

	void stop_streaming()
		{ DoStopStreaming();};

	enum state{
		st_Initialization,
		st_InitializationError,
		st_ConnectError,	//for network cams
		st_CaptureError,
		st_Ready,
		st_Setup
	};
	state get_state() const ;


	struct frame{
		AVFrame* avframe;
		boost::chrono::steady_clock::time_point tp;

		boost::shared_array<unsigned char> frame_data;
		void* opaque_data;

		capturer* _capturer;

		state capturer_state;

		frame(capturer* c = 0);
		~frame();
	};
	typedef boost::shared_ptr<frame> frame_ptr;
	frame_ptr get_frame(boost::chrono::steady_clock::time_point last_frame_tp);


protected:


	struct capabilities{

		enum _flags{
			f_stop_streaming	= 1 << 0
		};

		unsigned int flags;

		//поддерживаемые Controls и их текущие значения
		//std::deque<CameraControl> controls;

		capabilities();
	};


	virtual void DoConnect3(const connect_parameters& params) = 0;
	virtual void DoDisconnect() = 0;
	//frame operations
	virtual frame_ptr DoGetFrame3(boost::chrono::steady_clock::time_point last_frame_tp) = 0;
	virtual void DoReturnFrame3(boost::chrono::steady_clock::time_point tp, void* opaque){};

	virtual format DoGetCurrentFormat() const = 0;
	virtual void DoSetFramesize(const frame_size& fsize) = 0;
	virtual frame_size DoGetFramesize() = 0;
	virtual definition DoGetDefinition() const = 0;
	virtual capabilities DoGetCapabilities() const = 0;

	virtual void DoStopStreaming() {};

	virtual void on_state_change(state old_state,state new_state){};


private:
	
	AVFrame* convert_pix_fmt(AVFrame* src,AVPixelFormat dst_pix_fmt,std::string& error_message);

	void return_frame(boost::chrono::steady_clock::time_point tp, void* opaque);


	boost::atomic<state> capturer_state;
	void set_state(state _state);

};



#endif//__capturer_h__
