#include "camera_opencv.h"

camera_opencv::camera_opencv(const capturer::connect_parameters& _cp,state_change_handler _state_h, stop_handler _stop_h):
		camera(_cp,_state_h,_stop_h){


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
	capturer::connect_parameters cp = get_connect_parameters();
	if (cp.connection_string==std::string(WindowsCameraDevname)){
		_definition.bus_info = "windows";
		_definition.device_name = "internal_camera";
		_definition.manufacturer_name = "lenovo";
	} else{
		_definition.bus_info = "ip-network";
		_definition.device_name = "mjpeg-camera";
		_definition.manufacturer_name = "unknown";
	}
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

		if (get_connect_parameters().connection_string==std::string(WindowsCameraDevname))
			cap.open(0); else
			cap.open(get_connect_parameters().connection_string);

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




