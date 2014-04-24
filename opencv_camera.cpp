#include "opencv_camera.h"

opencv_camera::opencv_camera(const std::string& _dev_name_or_doc_id, couchdb::manager* _cdb_manager,stop_handler _h,
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

opencv_camera::~opencv_camera(){
	cap = cv::VideoCapture();
}

capturer::definition opencv_camera::DoGetDefinition() const{
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
capturer::capabilities opencv_camera::DoGetCapabilities() const{
	capabilities caps;
	caps.flags = 0;

	return caps;
}


void opencv_camera::DoConnect3(const capturer::connect_parameters& params){
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
		current_format.framesizes.push_back(current_framesize);


	}
	catch(...){
		throw std::runtime_error("connect error");
	}

}

void opencv_camera::DoDisconnect(){
	boost::unique_lock<boost::mutex> l1(internal_mutex);

	cap = cv::VideoCapture();
}




capturer::frame_ptr opencv_camera::DoGetFrame3(boost::chrono::steady_clock::time_point last_frame_tp){

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

capturer::format opencv_camera::DoGetCurrentFormat() const {
	return current_format;
}

void opencv_camera::DoSetFramesize(const frame_size& fsize){

}

frame_size opencv_camera::DoGetFramesize(){
	return current_framesize;
}




