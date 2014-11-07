#ifndef __camera_openmax_h__
#define __camera_openmax_h__

#include "camera.h"

class camera_openmax: public camera{
public:
	camera_openmax(const capturer::connect_parameters& _cp,state_change_handler _state_h, stop_handler _stop_h);	
	~camera_openmax();

	class Impl;
protected:
	void do_connect(const connect_parameters& params);
	void do_disconnect();

	frame_ptr do_get_frame(boost::chrono::steady_clock::time_point last_frame_tp);
	void do_return_frame(boost::chrono::steady_clock::time_point tp, void* opaque);

	format do_get_current_format() const;

	frame_size do_get_framesize();
	void do_set_framesize(const frame_size& fsize);
	
	definition do_get_definition() const;
	capabilities do_get_capabilities() const;

	void do_start_streaming();
	void do_stop_streaming();


private:	
	Impl* pimpl;

};

#endif//__camera_openmax_h__