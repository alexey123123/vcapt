#ifndef __opencv_camera_h__
#define __opencv_camera_h__

#include <opencv2/opencv.hpp>

#include "camera.h"

static const char* WindowsCameraDevname="video_windows";

class camera_opencv: public camera{
public:
	
	
	camera_opencv(const capturer::connect_parameters& _cp,state_change_handler _state_h, stop_handler _stop_h);
	~camera_opencv();

protected:

	void DoDisconnect();

	void DoSetControl(const CameraControl& c, const std::string& new_value)
	{};



	bool IsInitialized()
	{return cap.isOpened();};



	void DoConnect3(const capturer::connect_parameters& params);
	frame_ptr DoGetFrame3(boost::chrono::steady_clock::time_point last_frame_tp);
	format DoGetCurrentFormat() const ;
	void DoSetFramesize(const frame_size& fsize);
	frame_size DoGetFramesize();

	definition DoGetDefinition() const;
	capabilities DoGetCapabilities() const;


private:
	cv::VideoCapture cap;
	frame_size current_framesize;
	format current_format;
	boost::mutex internal_mutex;
};

#endif//__opencv_camera_h__