#include <boost/foreach.hpp>

#include <system/Platform.h>

#include "libav.h"
#include "capturer.h"




struct fmt_map {
	enum PixelFormat ff_fmt;
	enum AVCodecID codec_id;
	uint32_t v4l2_fmt;
};


capturer::format::format():
ffmpeg_pixfmt(AV_PIX_FMT_NONE),
	need_codec_id(AV_CODEC_ID_NONE),
	v4l2_pixfmt(0),
	type(fst_unknown),
	width_min(0),
	width_max(0),
	width_step(1),
	height_min(0),
	height_max(0),
	height_step(1){

}

bool capturer::format::defined() const{
	if (v4l2_pixfmt != 0)
		return true;
	if (ffmpeg_pixfmt != AV_PIX_FMT_NONE)
		return true;
	return false;
}

capturer::~capturer(){
}

void capturer::connect(const connect_parameters& params){
	try{
		DoConnect3(params);
		set_state(st_Ready);

		return ;
	}
	catch(std::runtime_error& ex){
		//TODO: journal
	}
	set_state(st_ConnectError);	
}
void capturer::disconnect(){
	try{
		DoDisconnect();
	}
	catch(...){
		//TODO: journal
	}

}

capturer::frame_ptr capturer::get_frame(boost::chrono::steady_clock::time_point last_frame_tp){
	frame_ptr fptr = DoGetFrame3(last_frame_tp);

	if (!fptr){
		fptr = frame_ptr(new frame());
		fptr->tp = boost::chrono::steady_clock::now();
	}		

	fptr->capturer_state = get_state();

	return fptr;
}

AVFrame* capturer::convert_pix_fmt(AVFrame* src,AVPixelFormat dst_pix_fmt,std::string& error_message){
	SwsContext* c = 0;
	bool ret = true;
	AVFrame* ret_frame = 0;

	AVPicture pic_bgr24;
	try{

		//TODO: SwsContext cashe
		c =  libav::sws_getContext( src->width,src->height, 
			AVPixelFormat(src->format),
			src->width,src->height,dst_pix_fmt, SWS_BICUBIC, NULL, NULL, NULL );
		if (!c)
			throw std::runtime_error("cannot allocate SwsContext");

		int res=-55;

		ret_frame = libav::av_frame_alloc();
		ret_frame->width = src->width;
		ret_frame->height = src->height;
		ret_frame->format = dst_pix_fmt;
		libav::av_frame_get_buffer(ret_frame,1);

		res = libav::sws_scale( c ,
			src->data, src->linesize, 
			0, 
			src->height,
			ret_frame->data, ret_frame->linesize ); 

		ret_frame->format = dst_pix_fmt;

	}
	catch(std::runtime_error& ex){
		error_message = std::string(ex.what());
		if (ret_frame != 0){
			libav::av_frame_free(&ret_frame);
		}
		ret_frame = 0;
	}


	if (c)
		libav::sws_freeContext(c);

	return ret_frame;

}


capturer::frame::frame(capturer* c):_capturer(c),avframe(0),opaque_data(0){

};
capturer::frame::~frame(){
	if (_capturer)
		_capturer->return_frame(tp,opaque_data);
	if (avframe)
		libav::av_frame_free(&avframe);
}

void capturer::return_frame(boost::chrono::steady_clock::time_point tp, void* opaque){
	DoReturnFrame3(tp,opaque);
}


void capturer::set_framesize(const frame_size& fsize){
	try{
		set_state(st_Initialization);
		DoSetFramesize(fsize);
		set_state(st_Ready);

		return ;
	}
	catch(std::runtime_error& ex){
		//TODO: journal
	}
	set_state(st_InitializationError);	

};


void capturer::set_state(state _state){
	state prev_value = capturer_state.exchange(_state,boost::memory_order_release);

	if (_state != prev_value){
		on_state_change(prev_value,_state);
	}

}

capturer::state capturer::get_state() const {
	return capturer_state;
}

bool capturer::format::check_framesize(const frame_size& fs) const{
	switch(type){
		case fst_discrete:
			if (std::find(framesizes.begin(),framesizes.end(),fs) == framesizes.end())
				return false;
			break;
		case fst_stepwise:
			if (fs.width < fs.height)
				return false;

			if (fs.width < width_min)
				return false;
			if (fs.width > width_max)
				return false;
			if (width_step > 0)
				if ((fs.width % width_step) != 0)
					return false;

			if (fs.height < height_min)
				return false;
			if (fs.height > height_max)
				return false;
			if (height_step > 0)
				if ((fs.height % height_step) != 0)
					return false;

			break;
		default:
			//let's try it!
			break;
	}

	return true;
}


std::deque<frame_size> capturer::format::get_possible_framesizes() const{
	std::deque<frame_size> ret;

	switch(type){
	case fst_discrete:
		return framesizes;
	case fst_stepwise:{

		if (check_framesize(frame_size(320,200)))
			ret.push_back(frame_size(320,200));
		if (check_framesize(frame_size(640,480)))
			ret.push_back(frame_size(640,480));
		if (check_framesize(frame_size(800,600)))
			ret.push_back(frame_size(800,600));
		if (check_framesize(frame_size(1024,768)))
			ret.push_back(frame_size(1024,768));
		if (check_framesize(frame_size(1280,720)))
			ret.push_back(frame_size(1280,720));
		if (check_framesize(frame_size(1920,1080)))
			ret.push_back(frame_size(1920,1080));


		}
	}
	return ret;
}

capturer::capabilities::capabilities():flags(0){

}
