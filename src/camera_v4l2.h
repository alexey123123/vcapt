#ifndef __V4L2_Capturer_h__
#define __V4L2_Capturer_h__

#include <map>

#include <boost/atomic.hpp>

#include <system/Platform.h>

#include "types.h"
#include "camera.h"


class camera_v4l2: public camera{
public:
	camera_v4l2(const capturer::connect_parameters& _cp,state_change_handler _state_h, stop_handler _stop_h);
	~camera_v4l2();



	static bool read_v4l2_device_definition(const std::string& devname,int fd,capturer::definition& def);

protected:
	void DoSetControl(const CameraControl& c, const std::string& new_value);

	bool IsInitialized()
		{return fd != 0;};

	

	//new iface

	void do_connect(const capturer::connect_parameters& params);
	void do_disconnect();
	frame_ptr do_get_frame(boost::chrono::steady_clock::time_point last_frame_tp);
	void do_return_frame(boost::chrono::steady_clock::time_point tp, void* opaque);
	format do_get_current_format() const 
		{return selected_format;};
	frame_size do_get_framesize()
		{return current_frame_size;}

	void do_set_framesize(const frame_size& fsize);

	void do_stop_streaming();

	definition do_get_definition() const;
	capabilities do_get_capabilities() const;

private:
	int fd;
	std::string videodev_filename;

	capturer::definition v4l2_device_definition;
	capturer::capabilities v4l2_device_capabilities;

	boost::recursive_mutex internal_mutex;

	//selected format
	capturer::format selected_format;
	frame_size current_frame_size;


	enum v4l2_camera_state{
		vs_initialization,
		vs_streaming,
		vs_stopped
	};


	v4l2_camera_state streaming_state;

	int current_sizeimage;//текущий размер кадра, под который выделен буффер

	std::size_t maximum_buffer_size_mb;
	struct buffer {
		int						index;
		void *                  start;
		std::size_t             length;

		bool extracted;
		buffer():extracted(false){};
	};
	boost::shared_array<buffer> buffers;
	std::size_t n_buffers;
	int dequeued_buffers_count;



	/*
	bool wait_for_frame(std::string& error_message);//select на fd - ожидание фрейма
	bool extract_completed_frame(DeviceWorkContext::buffer& frame_buffer,std::string& error_message);//вытащить frame из очереди для обработки
	bool return_frame_to_queue(const DeviceWorkContext::buffer& frame_buffer,std::string& error_message);
	*/
	void unmap_buffers();
	void close_fd();
	bool wait_all_extracted_buffers(boost::chrono::steady_clock::duration max_duration);

	//системные имена control'ов и их v4l2-идентификаторы
	std::map<std::string,unsigned int> control_sys_names_and_v4l2_ids;

	enum e_connection_type{
		ct_internal_iface,
		ct_usb,
		ct_network_mjpeg
	};
	e_connection_type connection_type;

	bool try_enum_formats(std::deque<format>& _formats);

	frame_ptr last_frame;
	boost::mutex get_frame_mutex;

	frame_ptr get_frame3();

};

#endif//__V4L2_Capturer_h__
