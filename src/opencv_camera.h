#ifndef __opencv_camera_h__
#define __opencv_camera_h__

#include <opencv2/opencv.hpp>

#include "camera.h"

class opencv_camera: public camera{
public:
	opencv_camera(const std::string& _dev_name_or_doc_id, couchdb::manager* _cdb_manager,stop_handler _h,
		bool _windows_internal_cam = true);
	~opencv_camera();

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
	bool windows_internal_cam;
	cv::VideoCapture cap;
	frame_size current_framesize;
	format current_format;
	boost::mutex internal_mutex;
};

#endif//__opencv_camera_h__