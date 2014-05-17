#include "camera_opencv.h"

camera_opencv::camera_opencv(const std::string& _dev_name_or_doc_id, couchdb::manager* _cdb_manager,stop_handler _h,
	bool _windows_internal_cam):
		camera(
			_windows_internal_cam? camera::c_local : camera::c_network,
			_dev_name_or_doc_id,_cdb_manager,_h),
	windows_internal_cam(_windows_internal_cam){


		current_format.ffmpeg_pixfmt = AV_PIX_FMT_BGR24;
		current_format.v4l2_pixfmt = 0;
		current_format.need_codec_id = AV_CODEC_ID_NONE;
		current_format.type = format::fst_discrete;
		//we don't know framesize yet....
		current_framesize = frame_size(640,480);
		current_format.framesizes.push_back(current_framesize);


}

camera_opencv::~camera_opencv(){
	cap = cv::VideoCapture();
}

capturer::definition camera_opencv::DoGetDefinition() const{
	capturer::definition _definition;
	if (windows_internal_cam){
		_definition.bus_info = "windows";
		_definition.device_name = "internal_camera";
		_definition.manufacturer_name = "lenovo";
	} else{
		_definition.bus_info = "ip-network";
		_definition.device_name = "mjpeg-camera";
		_definition.manufacturer_name = "unknown";
	}
	_definition.unique_string = _definition.manufacturer_name+"_"+_definition.device_name+"_"+_definition.bus_info;
	return _definition;
}
capturer::capabilities camera_opencv::DoGetCapabilities() const{
	capabilities caps;
	caps.flags = 0;

	return caps;
}


void camera_opencv::DoConnect3(const capturer::connect_parameters& params){
	boost::unique_lock<boost::mutex> l1(internal_mutex);

	try{

		if (windows_internal_cam)
			cap.open(0); else
			cap.open(params.connection_string);

		if (!cap.isOpened())
			throw std::runtime_error("");
	}
	catch(...){		
		throw std::runtime_error("connect error");
	}





	try{

		cv::Mat m1;
		cap >> m1;


		current_framesize = frame_size(m1.cols,m1.rows);

		current_format.framesizes.clear();
		current_format.framesizes.push_back(frame_size(320,240));
		current_format.framesizes.push_back(frame_size(640,480));


	}
	catch(...){
		throw std::runtime_error("connect error");
	}

}

void camera_opencv::DoDisconnect(){
	boost::unique_lock<boost::mutex> l1(internal_mutex);

	cap = cv::VideoCapture();
}




capturer::frame_ptr camera_opencv::DoGetFrame3(boost::chrono::steady_clock::time_point last_frame_tp){

	if (!cap.isOpened())
		return capturer::frame_ptr();

	boost::unique_lock<boost::mutex> l1(internal_mutex);



	capturer::frame_ptr fptr;

	try{
		cv::Mat mat1;
		cap >> mat1;
		

		//convert to BGR24

		fptr = capturer::frame_ptr(new capturer::frame());
		fptr->tp = boost::chrono::steady_clock::now();

		unsigned int frame_data_size = mat1.cols * mat1.rows * 3;// 3 bytes per pixel
		unsigned char* arr = new unsigned char[frame_data_size];
		fptr->frame_data = boost::shared_array<unsigned char>(arr);
		for (int i = 0; i < mat1.rows; i++)
		{
			memcpy( &( arr[ i*mat1.cols*3 ] ), &( mat1.data[ i*mat1.step ] ), mat1.cols*3 );
		}
		//construct AVframe
		fptr->avframe = libav::av_frame_alloc();
		fptr->avframe->width = mat1.cols;
		fptr->avframe->height = mat1.rows;
		fptr->avframe->format = AV_PIX_FMT_BGR24;
		int _size = libav::avpicture_fill((AVPicture *)fptr->avframe, arr, 
			(AVPixelFormat)fptr->avframe->format, 
			fptr->avframe->width, 
			fptr->avframe->height);
	}
	catch(...){
		if (!cap.isOpened()){
			throw std::runtime_error("cannot capture frame");
		}
	}
	return fptr;

}

capturer::format camera_opencv::DoGetCurrentFormat() const {
	return current_format;
}

void camera_opencv::DoSetFramesize(const frame_size& fsize){
	boost::unique_lock<boost::mutex> l1(internal_mutex);
	cap.set(CV_CAP_PROP_FRAME_WIDTH, fsize.width);
	cap.set(CV_CAP_PROP_FRAME_HEIGHT, fsize.height);
	current_framesize = fsize;
}

frame_size camera_opencv::DoGetFramesize(){
	return current_framesize;
}




