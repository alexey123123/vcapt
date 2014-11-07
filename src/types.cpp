#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>


#include <utility/Pack.h>

#include "types.h"
#include "couchdb.h"
#include "camera_container.h"


std::string frame_size::to_string() const{
	std::ostringstream oss;
	oss << width << "x" << height;
	return oss.str();
}
bool frame_size::from_string(const std::string& s){
	try{
		std::deque<std::string> parts = Utility::Pack::__parse_string_by_separator__(s,"x");
		if (parts.size() != 2)
			throw std::runtime_error("");

		width = boost::lexical_cast<int>(parts[0]);
		height = boost::lexical_cast<int>(parts[1]);

		return true;
	}
	catch(...){

	}
	return false;

}
void frame_size::from_string(const std::string& s,const frame_size& default_value){
	try{
		std::deque<std::string> parts = Utility::Pack::__parse_string_by_separator__(s,"x");
		if (parts.size() != 2)
			throw std::runtime_error("");

		width = boost::lexical_cast<int>(parts[0]);
		height = boost::lexical_cast<int>(parts[1]);
	}
	catch(...){
		width = default_value.width;
		height = default_value.height;
	}
}


buffer::buffer():pt(boost::posix_time::not_a_date_time){};
buffer::buffer(packet_ptr p):_packet_ptr(p),pt(boost::posix_time::not_a_date_time){};
buffer::~buffer(){

}

std::size_t buffer::size() const{
	if (_packet_ptr)
		return _packet_ptr->p.size;
	return _data.size();
};

unsigned char* buffer::data(){
	if (_packet_ptr != 0)
		return _packet_ptr->p.data;
	return &_data[0];

}


tcp_client::~tcp_client(){
	boost::system::error_code ec;
	_socket.close(ec);
};

bool client_parameters::construct(tcp_client_ptr cptr,std::string& error_message){
	try{
		if (cptr->url_keypairs.find("type")==cptr->url_keypairs.end())
			throw std::runtime_error("undefined stream type");

		//	container name
		container_name = cptr->url_keypairs["type"];		

	
		f_size = boost::optional<frame_size>();

		/*
		int width = 0;
		if (cptr->url_keypairs.find("width")!=cptr->url_keypairs.end())
			try{
				width = boost::lexical_cast<int>(cptr->url_keypairs["width"]);
		}
		catch(...){
			throw std::runtime_error("width is incorrected");
		}

		int height = 0;
		if (cptr->url_keypairs.find("height")!=cptr->url_keypairs.end())
			try{
				height = boost::lexical_cast<int>(cptr->url_keypairs["height"]);
		}
		catch(...){
			throw std::runtime_error("width is incorrected");
		}

		if ((width != 0)&&(height!=0)){
			f_size = boost::optional<frame_size>(frame_size(width,height));
			std::cout<<"fsize:"<<f_size->to_string()<<std::endl;
		}
		*/
			

		
		//bitrate might be undefined (it's a network client)
		bitrate = boost::optional<int>();
		int b = 0;
		if (cptr->url_keypairs.find("bitrate")!=cptr->url_keypairs.end())
			try{
				b = boost::lexical_cast<int>(cptr->url_keypairs["bitrate"]);
				bitrate = boost::optional<int>(b);
				std::cout<<"bitrate:"<<b<<std::endl;
			}
			catch(...){
				throw std::runtime_error("bitrate incorrected");
			}

		autostop_timeout_sec = boost::optional<int>();
		if (cptr->url_keypairs.find("autostop_timeout")!=cptr->url_keypairs.end())
			try{
				b = boost::lexical_cast<int>(cptr->url_keypairs["autostop_timeout"]);
				autostop_timeout_sec = boost::optional<int>(b);
				std::cout<<"autostop_timeout_sec:"<<b<<std::endl;
		}
		catch(...){
			throw std::runtime_error("incorrect autostop_timeout_sec");
		}


		return true;
	}
	catch(std::runtime_error& ex){
		error_message = std::string(ex.what());
	}
	return false;


}

bool client_parameters::construct(couchdb::document_ptr d,std::string& error_message){
	try{
		//	container name
		container_name = d->get_property<std::string>(cprops::streams_video_format,"");
		if (container_name=="")
			container_name = "avi";


		f_size = boost::optional<frame_size>();
		//cannot set framesize for streams
		/*
		std::string s1;
		s1 = d->get_property<std::string>(cprops::streams_video_framesize,"");
		frame_size f1;
		f1.from_string(s1,frame_size());
		if (f1 != frame_size())
			f_size = boost::optional<frame_size>(f1);
		*/

		//bitrate is strongly expected
		bitrate = boost::optional<int>();
		int b = d->get_property<int>(cprops::streams_video_bitrate,-1);
		if (b==-1)
			throw std::runtime_error("undefined bitrate");
		if ((b < 50000)||(b>3000000))
			throw std::runtime_error("bitrate is out of range");
		bitrate = boost::optional<int>(b);


		return true;
	}
	catch(std::runtime_error& ex){
		error_message = std::string(ex.what());
	}
	return false;
}

packet::packet():stream_header_data(false),packet_flags(0){
	libav::av_init_packet(&p);
}
packet::~packet(){
	libav::av_free_packet(&p);
}

file_document_wrapper::file_document_wrapper(couchdb::document_ptr d):file_doc(d){
	file_doc->set_property<int>(file_document_props::file_state,fs_in_progress);
}
file_document_wrapper::~file_document_wrapper(){
	file_doc->set_property<int>(file_document_props::file_state,fs_incompleted);
}
void file_document_wrapper::mark_as_complete(){
	file_doc->set_property<int>(file_document_props::file_state,fs_completed);
}


void filestream_params::from_doc(couchdb::document_ptr d){
	std::string error_message;
	if (!c_params.construct(d,error_message))
		throw std::runtime_error(error_message);

	c_params.f_size = frame_size();//currently we not support filestreams with different framesizes

	max_filesize_mb = boost::optional<boost::int64_t>();
	boost::int64_t v = d->get_property<boost::int64_t>(cprops::streams_video_max_size,0);
	if (v!=0)
		max_filesize_mb = boost::optional<boost::int64_t>(v);

	max_duration_mins = boost::optional<int>();
	int v1 = d->get_property<int>(cprops::streams_video_max_duration,0);
	if (v1 != 0)
		max_duration_mins = boost::optional<int>(v1);

	if ((!max_filesize_mb)&&(!max_duration_mins))
		throw std::runtime_error("not defined filesize or duration");


}




boost::int64_t filestream_params::get_assumed_filesize() const {
	boost::int64_t max_fsize = -1;
	if (max_filesize_mb){
		//it's defined!
		max_fsize = (*max_filesize_mb) * 1024 * 1024;
	} else{
		//calculate from bitrate
		int nsec = (*max_duration_mins) * 60;
		max_fsize = nsec * ((*c_params.bitrate) / 8);
	}
	return max_fsize;

}

camera_controls::~camera_controls(){
	char* p = camera_name;
	if (p!=0)
		delete p;
}

void camera_controls::update(couchdb::document_ptr d){

	std::deque<std::string> filter_parts;

	int A = d->get_property<int>(cprops::controls_DisplayCameraName,0);
	if (A==1){
		need_display_camera_name.exchange(true,boost::memory_order_release);
	} else{
		need_display_camera_name.exchange(false,boost::memory_order_release);
	}

	std::string camera_name_str = d->get_property<std::string>(cprops::controls_CameraName,"");
	char* p = camera_name;
	if (p){
		std::string old_name(p);
		if (old_name != camera_name_str){
			delete p;
			camera_name.exchange(0,boost::memory_order_release);
		}
	}
	if (camera_name_str!=""){
		int l = camera_name_str.size() + 1;
		p = new char[l];
		memset(p,0,l);
		memcpy(p,camera_name_str.c_str(),camera_name_str.size());
		camera_name.exchange(p,boost::memory_order_release);

		std::string ss = "drawtext=x=(w*0.02):y=(h-text_h/2-line_h):fontsize=20:fontcolor=white:text='"+camera_name_str+"':fontfile="+FontsPath+"couri.ttf:box=1:boxcolor=black";
		filter_parts.push_back(ss);


	}

	bool f = d->get_property<int>(cprops::controls_WriteDateTimeText,0) == 1;
	need_display_timestamp.exchange(f,boost::memory_order_release);
	if (f){
		std::string ss = "drawtext=x=(w - text_w - w*0.02):y=(h-text_h/2-line_h):fontsize=20:fontcolor=white:text=%{localtime}:fontfile="+FontsPath+"couri.ttf:box=1:boxcolor=black";
		filter_parts.push_back(ss);
	}

	A = d->get_property<int>(cprops::controls_RotateAngle,0);
	if (A<0)
		A = 360 - (A*(-1));
	if (A>360)
		A = A % 360;
	rotate_angle.exchange(A,boost::memory_order_release);
	if (A!=0){
		std::ostringstream oss;
		oss<<"rotate="<<rotate_angle<<"*PI/180";
		filter_parts.push_back(oss.str());
	}
}


frame::frame():avframe(0),rotated(false){

};
frame::~frame(){
	if (dcb)
		dcb();

	if (avframe)
		libav::av_frame_free(&avframe);

}

frame_size frame::get_framesize() const{
	return avframe ? frame_size(avframe->width,avframe->height) : frame_size();
}


class frame_helper::Impl{
public:
	Impl(){};
	~Impl(){};

	AVFrame* resize_and_convert_format(AVFrame* src,int dst_width,int dst_height, AVPixelFormat dst_pixfmt,std::string& error_message);
	struct __resize_context{
		int src_width;
		int src_height;
		AVPixelFormat src_pix_fmt;
		int dst_width;
		int dst_height;
		AVPixelFormat dst_pix_fmt;

		SwsContext* c;


		__resize_context();
		~__resize_context();

		void update(
			int _src_width,
			int _src_height,
			PixelFormat _src_pix_fmt,
			int _dst_width,
			int _dst_height,
			PixelFormat _dst_pix_fmt);

	} resize_context;

	AVFrame* rpi_resize(AVFrame* src,int dst_width,int dst_height, std::string& error_message);
};

frame_helper::frame_helper(){
	pimpl = new Impl();
}
frame_helper::~frame_helper(){
	delete pimpl;
}

bool frame_helper::check_conversion(frame_size src_fsize,AVPixelFormat src_fmt,frame_size dst_fsize,AVPixelFormat dst_fmt,std::string& error_message) const{
	if (dst_fmt != AV_PIX_FMT_NONE)
		if (src_fmt != dst_fmt){
			//need format convert 
#if defined(FMT_CONVERT_DISABLED)
			error_message = "pixel format conversion disabled";
			return false;
#endif
		}
		if (src_fsize != dst_fsize){
			//need framesize change
#if defined(RESIZE_DISABLED)
			error_message = "cannot resize frame";
			return false;
#endif
			if (dst_fsize > src_fsize){
				error_message = "cannot increment size of frame";
				return false;
			}
		}

	return true;
}

frame_ptr frame_helper::resize_and_convert(frame_ptr src, AVPixelFormat dst_fmt,
	frame_size dst_fsize, std::string& error_message){
	frame_ptr ret_f;
	try{
		std::string e_string;
		if (!check_conversion(src->get_framesize(),(AVPixelFormat)src->avframe->format,dst_fsize,dst_fmt,e_string))
			throw std::runtime_error(e_string);

		AVFrame* avframe_to_encode = 0;
#if defined(OPENMAX_IL)
		//resize image with OpenMAX
#else
		//resize image with libav
		std::string error_message;
		avframe_to_encode = pimpl->resize_and_convert_format(
				src->avframe,dst_fsize.width,dst_fsize.height,dst_fmt,error_message);
		if (!avframe_to_encode)
			throw std::runtime_error("resize frame error:"+error_message);
#endif
		ret_f = frame_ptr(new frame());
		*(ret_f.get()) = *(src.get());
		ret_f->frame_data = boost::shared_array<unsigned char>();
		ret_f->avframe = avframe_to_encode;

	}
	catch(std::runtime_error& ex){
		error_message = std::string(ex.what());
		ret_f = frame_ptr();
	}

	return ret_f;

}

frame_helper::Impl::__resize_context::__resize_context(){
	c = 0;
	src_width = 0;
	src_height = 0;
	src_pix_fmt = AV_PIX_FMT_NONE;
	dst_width = 0;
	dst_height = 0;
	dst_pix_fmt = AV_PIX_FMT_NONE;


}
frame_helper::Impl::__resize_context::~__resize_context(){
	if (c!=0)
		libav::sws_freeContext(c);
}

void frame_helper::Impl::__resize_context::update(
	int _src_width,
	int _src_height,
	PixelFormat _src_pix_fmt,
	int _dst_width,
	int _dst_height,
	PixelFormat _dst_pix_fmt){

		if ((_src_width==src_width) &&
			(_src_height == src_height) &&
			(_src_pix_fmt == src_pix_fmt) &&
			(_dst_width == dst_width) &&
			(_dst_height == dst_height) &&
			(_dst_pix_fmt == dst_pix_fmt))
			return ;

		if (c!=0)
			libav::sws_freeContext(c);

		c =  libav::sws_getContext( _src_width, _src_height, 
			_src_pix_fmt,
			_dst_width, _dst_height, 
			_dst_pix_fmt, SWS_BICUBIC, NULL, NULL, NULL );
		if (!c)
			throw std::runtime_error("cannot allocate SwsContext");

}


AVFrame* frame_helper::Impl::resize_and_convert_format(AVFrame* src,int dst_width,int dst_height, AVPixelFormat dst_pixfmt,std::string& error_message){
	AVFrame* dst = 0;
	int res=-55;
	try{
		resize_context.update(src->width,src->height,(PixelFormat)src->format,dst_width,dst_height,dst_pixfmt);


		dst = libav::av_frame_alloc();
		dst->width = dst_width;
		dst->height = dst_height;
		dst->format = dst_pixfmt;
		libav::av_frame_get_buffer(dst,1);

		res = libav::sws_scale( resize_context.c ,
			src->data, src->linesize, 
			0, 
			src->height,
			dst->data, dst->linesize ); 
		dst->pts = src->pts;
	}
	catch(std::runtime_error& ex){
		if (dst != 0)
			libav::av_frame_free(&dst);
		error_message = std::string(ex.what());
	}





	return dst;

}

AVFrame* frame_helper::Impl::rpi_resize(AVFrame* src,int dst_width,int dst_height, std::string& error_message){
	AVFrame* ret_f = 0;
	try{
	}
	catch(std::runtime_error& ex){
		error_message = std::string(ex.what());
		if (ret_f)
			libav::av_frame_free(&ret_f);
	}
	return ret_f;
}


AVFrame* convert_cv_Mat_to_avframe_yuv420p(cv::Mat* cv_mat,int dst_width,int dst_height,std::string& error_message){
	SwsContext* c = 0;
	bool ret = true;
	AVFrame* ret_frame = 0;

	AVPicture pic_bgr24;
	try{
		//1. Convert cv::Mat to AVPicture
		std::vector<uint8_t> buf;
		buf.resize(cv_mat->cols * cv_mat->rows * 3); // 3 bytes per pixel
		for (int i = 0; i < cv_mat->rows; i++)
		{
			memcpy( &( buf[ i*cv_mat->cols*3 ] ), &( cv_mat->data[ i*cv_mat->step ] ), cv_mat->cols*3 );
		}

		libav::avpicture_fill(&pic_bgr24, &buf[0], AV_PIX_FMT_BGR24, cv_mat->cols,cv_mat->rows);
		//buf.clear();

		//2. BGR24 -> YUV420P


		c = libav::sws_getContext( cv_mat->cols,cv_mat->rows,
			AV_PIX_FMT_BGR24,
			dst_width, dst_height,AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL );
		if (!c)
			throw std::runtime_error("cannot allocate SwsContext");

		int res=-55;


		//ret_value = Frame::construct_and_alloc(dst_pixel_format,src->avframe()->width, src->avframe()->height,1);


		ret_frame = libav::av_frame_alloc();
		ret_frame->width = dst_width;
		ret_frame->height = dst_height;
		ret_frame->format = AV_PIX_FMT_YUV420P;
		libav::av_frame_get_buffer(ret_frame,1);

		res = libav::sws_scale( c ,
			pic_bgr24.data, pic_bgr24.linesize,
			0,
			cv_mat->rows,
			ret_frame->data, ret_frame->linesize );

		ret_frame->format = AV_PIX_FMT_YUV420P;

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



frame_ptr frame_helper::generate_black_frame(frame_size fs){
	using namespace cv;

	if (!fs)
		return frame_ptr();

	cv::Mat m1(fs.height, fs.width, CV_8UC3, Scalar(0,0,0));

	frame_ptr fptr;

	std::string error_message;
	AVFrame* av_f1 = convert_cv_Mat_to_avframe_yuv420p(&m1,fs.width,fs.height,error_message);
	if (!av_f1)
		return fptr;

	fptr = frame_ptr(new frame());
	fptr->avframe = av_f1;


	return fptr;

}

frame_generator::frame_generator(){

}


frame_ptr frame_generator::get(frame_size fs, 
	const std::string& title, 
	int frame_interval_ms,
	boost::chrono::steady_clock::time_point last_frame_tp){
		frame_ptr rd_frame;
		//try to find ready frame
		stored_frame sf;
		BOOST_FOREACH(sf,ready_frames){
			if (sf.get<0>() == fs)
				if (sf.get<1>() == title){
					rd_frame = sf.get<2>();
					break;
				}
		}

	if (rd_frame){
		if (frame_interval_ms > 0){
			
			typedef boost::chrono::duration<long,boost::milli> milliseconds;
			if (last_frame_tp > rd_frame->tp){
				milliseconds ms = boost::chrono::duration_cast<milliseconds>(last_frame_tp - rd_frame->tp);
				if (ms.count() < frame_interval_ms)
					return frame_ptr();
			}
		}
		boost::chrono::steady_clock::time_point now_tp = boost::chrono::steady_clock::now();
		rd_frame->tp = now_tp;
		return rd_frame;
	}
	//generate
	frame_ptr src_frame = _frame_helper.generate_black_frame(fs);
	//write error text
	filter fi;
	frame_ptr f(new frame());
	f->tp = boost::chrono::steady_clock::now();
	f->avframe = fi.draw_text_in_center(src_frame->avframe,title);	
	f->rotated = true;

	sf = boost::make_tuple<frame_size,std::string,frame_ptr>(fs,title,f);
	ready_frames.push_back(sf);

	return f;
}


void frame_generator::cleanup(boost::posix_time::time_duration td){
	std::deque<stored_frame> remained_frames;
	stored_frame sf;
	boost::chrono::steady_clock::time_point now_tp = boost::chrono::steady_clock::now();
	typedef boost::chrono::duration<long,boost::milli> milliseconds;
	BOOST_FOREACH(sf,ready_frames){
		frame_ptr f = sf.get<2>();
		bool need_erase = false;
		if (f->tp < now_tp){
			milliseconds ms = boost::chrono::duration_cast<milliseconds>(now_tp - f->tp);
			if (ms.count() > td.total_milliseconds()){
				//need erase
				need_erase = true;
				std::cout<<"frame_generator: <"<<sf.get<1>()<<"> cleaned"<<std::endl;
			}
		}
		if (!need_erase)
			remained_frames.push_back(sf);
	}
	ready_frames = remained_frames;

}


