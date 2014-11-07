#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/foreach.hpp>

#include <libavutil/pixfmt.h>

#include <system/Platform.h>

#include <utility/Journal.h>

#if defined(LinuxPlatform)

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>         /* for videodev2.h */
#include "videodev2.h"
#elif defined(Win32Platform)
#include "videodev2_win.h"
#endif


#include "camera_v4l2.h"

#define MAX_POSSIBLE_ERRORS 255
//ограничение на кол-во буфферов 
#define V4L2_BUFFERS_LIMIT 5
//кол-во кадров которое надо пропустить после изменения формата
const unsigned int SKIP_FRAMES_COUNT_AFTER_FORMAT_CHANGE=10;

int	__xioctl(int fd, int request, void *arg, const int* possible_errors = 0, int timeout_ms = 0)
{
	int r = -1;
	int i=0;
	boost::chrono::system_clock::time_point start;
	if (possible_errors!=0)
		if (timeout_ms>0)
			start = boost::chrono::system_clock::now();
	
	
#if defined(LinuxPlatform)
	bool f = false;
	do {
		f = false;
		
		r = ioctl (fd, request, arg);
		if (r != -1)
			break;
		//ищем r в possible_errors
		if (possible_errors)
			for (i=0;i!=MAX_POSSIBLE_ERRORS;i++){
				if (possible_errors[i]==0)
					break;
				f |= possible_errors[i] == errno;
				if (f)
					break;
			}
		if (f){
			//ошибка попала в список допустимых. проверем время
			f &= (timeout_ms>0);
			if (timeout_ms>0) {
				boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - start;
				f &= sec.count() *1000 < timeout_ms;
			}
		}

	}
	while (f);
#endif

	return r;
}


static int
	xioctl                          (int                    fd,
	int                    request,
	void *                 arg)
{
	int r=0;
#if defined(LinuxPlatform)
	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);
#endif
	return r;
}

int	xioctl2(int fd, int request, void *arg, const int* possible_errors, int timeout_ms)
{
	int r = -1;
	int i=0;
	boost::chrono::system_clock::time_point start;
	if (possible_errors!=0)
		if (timeout_ms>0)
			start = boost::chrono::system_clock::now();


#if defined(LinuxPlatform)
	bool f = false;
	do {
		f = false;

		r = ioctl (fd, request, arg);
		if (r != -1)
			break;
		//ищем r в possible_errors
		if (possible_errors)
			for (i=0;i!=MAX_POSSIBLE_ERRORS;i++){
				if (possible_errors[i]==0)
					break;
				f |= possible_errors[i] == errno;
				if (f)
					break;
			}
			if (f){
				//ошибка попала в список допустимых. проверем время
				f &= (timeout_ms>0);
				if (timeout_ms>0) {
					boost::chrono::duration<double> sec = boost::chrono::system_clock::now() - start;
					f &= sec.count() *1000 < timeout_ms;
				}
			}

	}
	while (f);
#endif

	return r;
}



#define CLEAR(x) memset (&(x), 0, sizeof (x))



namespace fs = boost::filesystem;



std::string __get_videodevice_slot_name2(const std::string& devpath){
	std::string ret_value = "unknown_slot";
	//выделяем из devpath имя устройства (videoX)
	std::string devname = fs::path(devpath).filename().string();
	//пытаемся прочитать /sys/class/video4linux/videoX/device
	fs::path p1("/sys/class/video4linux/"+devname+"/device");
	if (fs::is_symlink(p1)){
		//найдена симв. ссылка. читаем ее
		char buff[1024];
		memset(buff,0,1024);
#if defined(LinuxPlatform)
		int symlink_content_size = readlink(p1.string().c_str(), buff, 1024);
		if (symlink_content_size>0){
			p1 = fs::path(std::string(buff));
			ret_value = p1.filename().string();
		}
#endif


	}
	return ret_value;
}

std::string v4l2_pisxftm_fourcc_to_str2(unsigned int v4l2_pixfmt);



bool v4ls_do_start_streaming(int fd, int buffers_count, std::string& error_message){

	try{
#if defined(LinuxPlatform)

		for (int i = 0; i < buffers_count; ++i) {
			struct v4l2_buffer buf;

			CLEAR (buf);

			buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory      = V4L2_MEMORY_MMAP;
			buf.index       = i;

			if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
				throw std::runtime_error("VIDIOC_QBUF error:"+std::string(strerror(errno)));
		}


		int _type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl (fd, VIDIOC_STREAMON, &_type))
			throw std::runtime_error("VIDIOC_STREAMON error ("+std::string(strerror(errno))+")");
#endif	

		return true;
	}
	catch(std::runtime_error& ex){
		error_message = std::string(ex.what());
	}
	return false;

}
bool do_stop_streaming_v4l2(int fd, std::string& error_message){
#if defined(LinuxPlatform)
	if (fd != -1){
		int _type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &_type)){
			error_message = "VIDIOC_STREAMOFF error ("+std::string(strerror(errno))+")";
			return false;
		}
	}		
#endif
	return true;
}

camera_v4l2::camera_v4l2(const capturer::connect_parameters& _cp,state_change_handler _state_h, stop_handler _stop_h):
		camera(_cp, _state_h,_stop_h),
		n_buffers(0),
		dequeued_buffers_count(0),
		streaming_state(vs_stopped)
{
		//v4l2_device_capabilities.flags = capabilities::f_start_stop_streaming;
	v4l2_device_capabilities.flags = 0;
}


camera_v4l2::~camera_v4l2(){
	do_disconnect();
}


std::string __get_videodevice_slot_name(const std::string& devpath){
	std::string ret_value = "unknown_slot";
	std::string devname = fs::path(devpath).filename().string();
	fs::path p1("/sys/class/video4linux/"+devname+"/device");
	if (fs::is_symlink(p1)){

		char buff[1024];
		memset(buff,0,1024);
#if defined(LinuxPlatform)
		int symlink_content_size = readlink(p1.string().c_str(), buff, 1024);
		if (symlink_content_size>0){
			p1 = fs::path(std::string(buff));
			ret_value = p1.filename().string();
		}
#endif


	}
	return ret_value;
}


bool __v4l2_aditional_load_control__(int fd, struct v4l2_queryctrl &qctrl,v4l2_ext_control& ext_ctrl){

	struct v4l2_control ctrl = { 0 };
	struct v4l2_ext_controls ctrls = { 0 };


	memset(&ext_ctrl,0,sizeof(v4l2_ext_control));

	if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)
		return false;
	if (qctrl.type == V4L2_CTRL_TYPE_CTRL_CLASS) {
		return false;
	}
	ext_ctrl.id = qctrl.id;
	ctrls.ctrl_class = V4L2_CTRL_ID2CLASS(qctrl.id);
	ctrls.count = 1;
	ctrls.controls = &ext_ctrl;
	if (V4L2_CTRL_ID2CLASS(qctrl.id) != V4L2_CTRL_CLASS_USER && qctrl.id < V4L2_CID_PRIVATE_BASE) {
		if (xioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls))
			return false;
	}
	else {
		ctrl.id = qctrl.id;
		if (xioctl(fd, VIDIOC_G_CTRL, &ctrl))
			return false;
		ext_ctrl.value = ctrl.value;
	}

	return true;
}

struct fmt_map {
    enum PixelFormat ff_fmt;
    enum AVCodecID codec_id;
    uint32_t v4l2_fmt;
};


static struct fmt_map fmt_conversion_table[] = {
	//ff_fmt           codec_id           v4l2_fmt
	{ PIX_FMT_YUV420P, CODEC_ID_NONE, V4L2_PIX_FMT_YUV420  },
	{ PIX_FMT_YUV422P, CODEC_ID_NONE, V4L2_PIX_FMT_YUV422P },
	{ PIX_FMT_YUYV422, CODEC_ID_NONE, V4L2_PIX_FMT_YUYV    },
	{ PIX_FMT_UYVY422, CODEC_ID_NONE, V4L2_PIX_FMT_UYVY    },
	{ PIX_FMT_YUV411P, CODEC_ID_NONE, V4L2_PIX_FMT_YUV411P },
	{ PIX_FMT_YUV410P, CODEC_ID_NONE, V4L2_PIX_FMT_YUV410  },
	{ PIX_FMT_RGB555,  CODEC_ID_NONE, V4L2_PIX_FMT_RGB555  },
	{ PIX_FMT_RGB565,  CODEC_ID_NONE, V4L2_PIX_FMT_RGB565  },
	{ PIX_FMT_BGR24,   CODEC_ID_NONE, V4L2_PIX_FMT_BGR24   },
	{ PIX_FMT_RGB24,   CODEC_ID_NONE, V4L2_PIX_FMT_RGB24   },
	{ PIX_FMT_BGRA,    CODEC_ID_NONE, V4L2_PIX_FMT_BGR32   },
	{ PIX_FMT_GRAY8,   CODEC_ID_NONE, V4L2_PIX_FMT_GREY    },
	{ PIX_FMT_NV12,    CODEC_ID_NONE, V4L2_PIX_FMT_NV12    },

	//JPEG-форрматы расположены в самом низу, тк наблюдались проблемы с раскодированием
	//картинки на камере 
	{ PIX_FMT_NONE,    CODEC_ID_MJPEG, V4L2_PIX_FMT_MJPEG },
	{ PIX_FMT_NONE,    CODEC_ID_MJPEG, V4L2_PIX_FMT_JPEG }, 
};

static enum PixelFormat fmt_v4l2ff(uint32_t v4l2_fmt, enum AVCodecID codec_id)
{
	unsigned int i;

	for (i = 0; i < FF_ARRAY_ELEMS(fmt_conversion_table); i++) {
		if (fmt_conversion_table[i].v4l2_fmt == v4l2_fmt &&
			fmt_conversion_table[i].codec_id == codec_id) {
				return fmt_conversion_table[i].ff_fmt;
		}
	}

	return PIX_FMT_NONE;
}


static enum AVCodecID fmt_v4l2codec(uint32_t v4l2_fmt)
{
	unsigned int i;

	for (i = 0; i < FF_ARRAY_ELEMS(fmt_conversion_table); i++) {
		if (fmt_conversion_table[i].v4l2_fmt == v4l2_fmt) {
			return fmt_conversion_table[i].codec_id;
		}
	}

	return CODEC_ID_NONE;
}



static inline std::string v4l2_pisxftm_fourcc_to_str(unsigned int v4l2_pixfmt){
    char fourcc_ch[5];
	memset(fourcc_ch,0,5);
	memcpy(fourcc_ch,&v4l2_pixfmt,4);
	return std::string(fourcc_ch);
}

bool camera_v4l2::try_enum_formats(std::deque<format>& _formats){

#if defined(LinuxPlatform)
	//вытаскиваем все поддерживаемые форматы и размеры изображения
	//оставляем только те, что поддерживаются ffmpeg
	//выбираем RAW и FAST-форматы, отдаем допустимые разрешения
	struct v4l2_fmtdesc fmt_desc;
	int index = 0;
	
	std::ostringstream oss;
	bool mjpeg_format_found = false;
	capturer::format mjpeg_format;
	while (true){


		CLEAR (fmt_desc);
		fmt_desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt_desc.index = index;
		if (-1 == xioctl (fd, VIDIOC_ENUM_FMT, &fmt_desc)){
			std::cout<<"VIDIOC_ENUM_FMT(ind:"<<index<<") error ("<<strerror(errno)<<")"<<std::endl;
			return false;
		}
		index ++;
		//есть формат

		capturer::format _format;

		//поддерживается?
		unsigned int i;
		for (i = 0; i < FF_ARRAY_ELEMS(fmt_conversion_table); i++)
			if (fmt_conversion_table[i].v4l2_fmt == fmt_desc.pixelformat){
				_format.v4l2_pixfmt = fmt_desc.pixelformat;
				_format.need_codec_id = fmt_conversion_table[i].codec_id;
				_format.ffmpeg_pixfmt = fmt_conversion_table[i].ff_fmt;

				mjpeg_format_found = (fmt_desc.pixelformat == V4L2_PIX_FMT_JPEG)||(fmt_desc.pixelformat == V4L2_PIX_FMT_MJPEG);

				break;
			}

			if (!_format.defined())
				continue;

			//размеры
			struct v4l2_frmsizeenum fs_fmt;
			int ind1 = 0;
			while(true){
				CLEAR (fs_fmt);
				fs_fmt.pixel_format = fmt_desc.pixelformat;
				fs_fmt.index = ind1;

				if (-1 == xioctl (fd, VIDIOC_ENUM_FRAMESIZES, &fs_fmt)){
					std::cout<<"VIDIOC_ENUM_FRAMESIZES(ind:"<<ind1<<") error: "<<strerror(errno)<<std::endl;

					//TODO: BeagleBode driver BUG
					//frame_size fsize(640,480);
					//capabilities.formats_and_frame_sizes[av_pixfmt].push_back(fsize);
					break;
				}


				switch(fs_fmt.type){
				case V4L2_FRMSIZE_TYPE_DISCRETE:{
					std::cout<<"V4L2_FRMSIZE_TYPE_DISCRETE"<<std::endl;
					frame_size fsize;
					fsize.width = fs_fmt.discrete.width;
					fsize.height = fs_fmt.discrete.height;
					_format.type = capturer::format::fst_discrete;
					_format.framesizes.push_back(fsize);

					ind1 ++;
					break;
												}
				case V4L2_FRMSIZE_TYPE_STEPWISE:
				case V4L2_FRMSIZE_TYPE_CONTINUOUS:
					std::cout<<"V4L2_FRMSIZE_TYPE_STEPWISE or V4L2_FRMSIZE_TYPE_CONTINUOUS"<<std::endl;
					_format.type = capturer::format::fst_stepwise;
					_format.width_min = fs_fmt.stepwise.min_width;
					_format.width_max = fs_fmt.stepwise.max_width;
					_format.width_step = fs_fmt.stepwise.step_width;
					_format.height_min = fs_fmt.stepwise.min_height;
					_format.height_max = fs_fmt.stepwise.max_height;
					_format.height_step = fs_fmt.stepwise.step_height;


					break;
				}

				if (fs_fmt.type != V4L2_FRMSIZE_TYPE_DISCRETE)
					break;
			}

			if (_format.type==capturer::format::fst_unknown)
				continue;

			if (_format.type==capturer::format::fst_discrete){
				if (_format.framesizes.size()==0)
					continue;
				__fs_sorter fs1;
				std::sort(_format.framesizes.begin(),_format.framesizes.end(),fs1);
			}
			_formats.push_back(_format);
	}
#endif

	return true;

}

void camera_v4l2::do_disconnect(){
	boost::unique_lock<boost::recursive_mutex> l1(internal_mutex);
	do_stop_streaming();

	close_fd();
}

int open_v4l2_device(const std::string& devname){
	int fd = -1;
#if defined(LinuxPlatform)
	struct stat st;

	if (-1 == stat (devname.c_str(), &st))
		throw std::runtime_error("cannot identify "+devname);

	if (!S_ISCHR (st.st_mode))
		throw std::runtime_error(devname+" is not a device");

	fd = open (devname.c_str(), O_RDWR, 0);

	if (-1 == fd)
		throw std::runtime_error("cannot open "+devname);
#endif
	return fd;
}

bool camera_v4l2::read_v4l2_device_definition(const std::string& devname,int fd,capturer::definition& def){
	
	int self_opened_fd = -1;
	int work_fd = fd;
	bool _result = true;
	try{
		if (fd==-1){
			//need open and close device
			self_opened_fd = open_v4l2_device(devname);
			work_fd = self_opened_fd;
		}

#if defined(LinuxPlatform)
		struct v4l2_capability cap;



		if (-1 == xioctl (work_fd, VIDIOC_QUERYCAP, &cap))
			throw std::runtime_error(devname +" is no V4L2 device");

		if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
			throw std::runtime_error(devname+" is no video capture device");


		def.device_name = std::string((char*)cap.card);
		def.bus_info = std::string((char*)cap.bus_info);
		def.manufacturer_name = std::string((char*)cap.driver);
		def.slot_name = __get_videodevice_slot_name2(devname);
#endif
	}
	catch(...){
		_result = false;
	}
	if (self_opened_fd){
#if defined(LinuxPlatform)
		close(self_opened_fd);
#endif
	}
	return _result;




}


void camera_v4l2::do_connect(const capturer::connect_parameters& params){
	#if defined(LinuxPlatform)

	boost::unique_lock<boost::recursive_mutex> l1(internal_mutex);

	streaming_state = vs_initialization;

	fd = open_v4l2_device(get_connect_parameters().connection_string);


	maximum_buffer_size_mb = params.maximum_buffer_size_mb;
	videodev_filename = params.connection_string;



	std::cout<<"V4L2Capturer: device opened"<<std::endl;

	if (!read_v4l2_device_definition("",fd,v4l2_device_definition))
		throw std::runtime_error(get_connect_parameters().connection_string+" is incompatible device");

		/* Select video input, video standard and tune here. */

		struct v4l2_cropcap cropcap;
		struct v4l2_crop crop;		
		unsigned int min;
		CLEAR (cropcap);

		cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
			crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			crop.c = cropcap.defrect; /* reset to default */

			if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) {
				switch (errno) {
				case EINVAL:
					/* Cropping not supported. */
					break;
				default:
					/* Errors ignored. */
					break;
				}
			}
		} else {
			/* Errors ignored. */
		}

		std::deque<format> supported_formats;
		try_enum_formats(supported_formats);

		if (supported_formats.size()==0){

			
			//Some buggy drivers on some devices (example:BeagleBone Black + camera cape) need special dispatch.
#if defined(BBB)
			std::cout<<"BeagleBoneBlack special case"<<std::endl;
			capturer::format f;
			f.ffmpeg_pixfmt = AV_PIX_FMT_YUV422P;
			f.need_codec_id = AV_CODEC_ID_NONE;
			f.v4l2_pixfmt = V4L2_PIX_FMT_YUYV;
			f.type = capturer::format::fst_stepwise;
			f.width_min = 320;
			f.height_min = 200;
			f.width_max = 1280;
			f.height_max = 960;
			f.width_step = 2;
			f.height_step = 2;

			supported_formats.push_back(f);
			

#else
			throw std::runtime_error("no compatible capture formats");
#endif

			
		}


		//output
		capturer::format f1;
		BOOST_FOREACH(f1,supported_formats){
			std::cout<<v4l2_pisxftm_fourcc_to_str2(f1.v4l2_pixfmt)<<":";
			switch(f1.type){
				case capturer::format::fst_discrete:{
					frame_size fs;
					BOOST_FOREACH(fs,f1.framesizes)
						std::cout<<fs.to_string()<<",";
					break;
				}
				case capturer::format::fst_stepwise:{
					std::cout<<"("<<f1.width_min<<":"<<f1.height_min<<") - ("<<f1.width_max<<":"<<f1.height_max<<"), step ("<<f1.width_step<<":"<<f1.height_step<<")";
					break;
				}

			}
			std::cout<<std::endl;

		}




		connection_type = ct_internal_iface;
		if (v4l2_device_definition.bus_info.find(std::string("usb")) != std::string::npos)
			connection_type = ct_usb;


		//select capture format
		selected_format = format();
		switch(connection_type){
			case ct_usb:{

				//try to find MJPEG format
				format f;
				BOOST_FOREACH(f,supported_formats)
					if (f.need_codec_id == AV_CODEC_ID_MJPEG){
						selected_format = f;
						break;
					}				


				if (!selected_format.defined())
					throw std::runtime_error("no have JPEG/MJPEG format in USB camera");

				break;
			}
			case ct_internal_iface:{
				//select top format from fmt_conversion_table
				unsigned int i;
				for (i = 0; i < FF_ARRAY_ELEMS(fmt_conversion_table); i++){
					fmt_map fm = fmt_conversion_table[i];
					format f;
					BOOST_FOREACH(f,supported_formats){
						if (f.v4l2_pixfmt == fm.v4l2_fmt){
							selected_format = f;
							break;
						}
					}

					if (selected_format.defined())
						break;
				}
				break;
			}
		}


		if (!selected_format.defined())
			throw std::runtime_error("no supported formats");

		std::cout<<"selected format:"<<v4l2_pisxftm_fourcc_to_str2(selected_format.v4l2_pixfmt)<<"(v4l2)="<<
			libav::av_get_pix_fmt_name(selected_format.ffmpeg_pixfmt)<<"(ffmpeg)"<<std::endl;

		
		//TODO: enum v4l2-controls

#endif

}

void camera_v4l2::close_fd(){
#if defined(LinuxPlatform)
	close(fd);
	fd=-1;
#endif
}

void camera_v4l2::unmap_buffers(){
#if defined(LinuxPlatform)

	int i;

	//std::cout<<"-->unmap_buffers()"<<std::endl;
	if ((buffers!=0)&&(n_buffers != 0)){
		for (i = 0; i < n_buffers; ++i){
			//unmap
			if (-1 == munmap (buffers[i].start, buffers[i].length))
				throw std::runtime_error("munmap error("+std::string(strerror(errno))+")");
			
		}

		if (fd != -1){
			struct v4l2_requestbuffers req;
			CLEAR (req);
			req.count               = 0;
			req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			req.memory              = V4L2_MEMORY_MMAP;
			if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req))
				throw std::runtime_error("VIDIOC_REQBUFS error ("+std::string(strerror(errno))+")");

		}


		std::cout<<"buffers ("<<n_buffers<<") unmapped and destroyed"<<std::endl;

		n_buffers = 0;
		buffers = boost::shared_array<buffer>();
	}
#endif

}






void camera_v4l2::do_set_framesize(const frame_size& fsize){



	do_stop_streaming();

	boost::unique_lock<boost::recursive_mutex> l1(internal_mutex);

	if (fsize == frame_size()){
		current_frame_size = frame_size();
		return;
	}


#if defined(LinuxPlatform)
	if (fd<0)
		throw std::runtime_error("device not opened");

	if (!selected_format.defined())
		throw std::runtime_error("format not selected");


	frame_size fsize_to_setup;

	switch(selected_format.type){
		case format::fst_discrete:{
			//select equal or nearest greater format
			frame_size fs;
			BOOST_FOREACH(fs,selected_format.framesizes){
				if (fs >= fsize){
					fsize_to_setup = fs;
					break;
				}
			}
			break;
		}
		case format::fst_stepwise:{
			fsize_to_setup = fsize;
			break;
		}

	}

	if (fsize_to_setup == frame_size())
		throw std::runtime_error("format selection failed");

	std::cout<<"fsize to setup: "<<fsize_to_setup.to_string()<<std::endl;

	int buffers_count = 0;
	struct v4l2_format fmt;
	CLEAR (fmt);
	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = fsize_to_setup.width;
	fmt.fmt.pix.height      = fsize_to_setup.height;
	fmt.fmt.pix.pixelformat = selected_format.v4l2_pixfmt;
	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;	
	int errno_array[3] = {EINTR,EBUSY,0};
	if (-1 == __xioctl (fd, VIDIOC_S_FMT, &fmt,errno_array,5000))
        throw std::runtime_error("VIDIOC_S_FMT error ("+std::string(strerror(errno))+")");


	if ((!buffers)||(n_buffers == 0)){

		struct v4l2_format fmt;
		CLEAR (fmt);
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (-1 == xioctl (fd, VIDIOC_G_FMT, &fmt))
			throw std::runtime_error("VIDIOC_G_FMT error ("+std::string(strerror(errno))+")");

		std::cout<<"sizeimage="<<fmt.fmt.pix.sizeimage<<std::endl;

		//вычисляем количество буфферов, которые будем выделять в драйвере и map'ить
		int buffers_count = maximum_buffer_size_mb * 1000000 / fmt.fmt.pix.sizeimage;
		if (buffers_count > V4L2_BUFFERS_LIMIT)
			buffers_count = V4L2_BUFFERS_LIMIT;


		std::cout<<"try to allocate memory for "<<buffers_count<<" frames"<<std::endl;
		struct v4l2_requestbuffers req;

		CLEAR (req);

		req.count               = buffers_count;
		req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		req.memory              = V4L2_MEMORY_MMAP;

		if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req))
			throw std::runtime_error("VIDIOC_REQBUFS error ("+std::string(strerror(errno))+")");



		if (req.count < 2)
			throw std::runtime_error("Insufficient buffer memory");
		n_buffers = req.count;

		buffers = boost::shared_array<buffer>
			(new buffer[n_buffers]);
		

		for (int i = 0; i < n_buffers; ++i) {
			struct v4l2_buffer buf;

			CLEAR (buf);

			buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory      = V4L2_MEMORY_MMAP;
			buf.index       = i;

			if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
				throw std::runtime_error("VIDIOC_QUERYBUF error ("+std::string(strerror(errno))+")");

			buffers[i].index = buf.index;
			buffers[i].length = buf.length;
			buffers[i].start =
				mmap (NULL /* start anywhere */,
				buf.length,
				PROT_READ | PROT_WRITE /* required */,
				MAP_SHARED /* recommended */,
				fd, buf.m.offset);

			if (MAP_FAILED == buffers[i].start)
				throw std::runtime_error("mmap error ("+std::string(strerror(errno))+")");
			printf("mapped buffer[%d], size %d, start %08X\n",buf.index,buf.length,(int)buffers[i].start);

		}
	}

#endif
	std::cout<<"format changed"<<std::endl;
	
	

	if (streaming_state != vs_streaming){
		std::string error_message;
		if (!v4ls_do_start_streaming(fd,n_buffers,error_message))
			throw std::runtime_error("start streaming error:");

		std::cout<<"streaming started"<<std::endl;
		streaming_state = vs_streaming;
	}

		


	current_frame_size = fsize;

	if ((!buffers)||(n_buffers==0))
		throw std::runtime_error("buffers not mapped");



	//если формат изменился, или прооизошел перезапуск streamer'а - надо пропустить какое-то кол-во кадров
	if (connection_type==ct_usb)
		if (SKIP_FRAMES_COUNT_AFTER_FORMAT_CHANGE){
			int skip_count = SKIP_FRAMES_COUNT_AFTER_FORMAT_CHANGE;
			if (selected_format.need_codec_id == CODEC_ID_RAWVIDEO)
				skip_count = 1;

			for (unsigned int i=0;i!=skip_count;i++){
				try{
					frame_ptr fptr = get_frame3();
					if (fptr)				
						if (fptr->avframe)
							printf("skipped frame %d...\n",i+1);

				}
				catch(...){

				}
			}


		}

}




bool wait_for_frame(int fd,std::string& error_message){
#if defined(LinuxPlatform)
	try{
		error_message="";
		fd_set fds,error_fds;
		struct timeval tv;
		int r;

		FD_ZERO (&fds);
		FD_SET (fd, &fds);

		FD_ZERO (&error_fds);
		FD_SET (fd, &error_fds);

		//Timeout.
		tv.tv_sec = 20;
		tv.tv_usec = 0;

		r = select (fd + 1, &fds, NULL, &error_fds, &tv);

		if (-1 == r) {
			if (EINTR == errno)
				throw std::runtime_error("interrupted");

		}

		if (0 == r)
			return false;//ничего не дождались

		if (FD_ISSET(fd,&error_fds))
			throw std::runtime_error("Device error");

		if (!FD_ISSET(fd,&fds))
			return false;//ничего не дождались

		return true;
	}
	catch(std::runtime_error& ex){
		error_message = std::string(ex.what());
	}
#endif
	return false;
}





void camera_v4l2::do_return_frame(boost::chrono::steady_clock::time_point tp, void* opaque){

#if defined(LinuxPlatform)
	boost::unique_lock<boost::recursive_mutex> l1(internal_mutex);

	buffer* b = (buffer*)opaque;
	
	b->extracted = false;


	if (streaming_state != vs_streaming)
		return ;


	struct v4l2_buffer buf;
	CLEAR (buf);

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = b->index;
	buf.bytesused = 0;

	if (-1 == xioctl (fd, VIDIOC_QBUF, &buf)){
		std::cout<<"videodev error.close fd"<<std::endl;
		close_fd();
		throw std::runtime_error("VIDIOC_QBUF error ("+std::string(strerror(errno))+")");
	}
	//std::cout<<"returned buffer-"<<b->index<<std::endl;
	dequeued_buffers_count--;

#endif

}

frame_ptr camera_v4l2::do_get_frame(boost::chrono::steady_clock::time_point last_frame_tp){
	if (streaming_state != vs_streaming)
		return frame_ptr();

	boost::unique_lock<boost::recursive_mutex> l1(internal_mutex);

	if (streaming_state != vs_streaming)
		return frame_ptr();

	if (last_frame_tp != boost::chrono::steady_clock::time_point())
		if (last_frame)
			if (last_frame->tp > last_frame_tp)
				return last_frame;


	last_frame = frame_ptr();
	int try_count = 0;
	while (try_count < 3){

		last_frame = get_frame3();

		if (last_frame)
			break;

		try_count++;
	}
	return last_frame;
}

frame_ptr camera_v4l2::get_frame3(){


	if (streaming_state != vs_streaming)
		return frame_ptr();
	
	


	frame_ptr f3ptr;

		


	int try_count_limit = 10;

#if defined(LinuxPlatform)
	//делаем DQUEUE
	while (!f3ptr){
		try_count_limit--;
		if (try_count_limit==0)
			throw std::runtime_error("cannot get frame");
		

			

		struct v4l2_buffer buf;
		CLEAR (buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;

		int ret = xioctl (fd, VIDIOC_DQBUF, &buf);
		if (ret == -1) {
			switch (errno) {
			case EAGAIN:{
				std::string error_message;
				if (!wait_for_frame(fd,error_message))
					throw std::runtime_error("wait frame error:"+error_message);
				break;
						}
			case EIO:{
				std::cout<<"------------------------------EIO------------------------"<<std::endl;
				//вернулся плохой буффер. отдаем его назад
				int _index = buf.index;
				CLEAR (buf);

				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				buf.index = _index;
				xioctl (fd, VIDIOC_QBUF, &buf);//не анализируем на возм. ошибку


				break;
					 }

			default:{
				std::cout<<"videodev error.close fd"<<std::endl;
				close_fd();
				throw std::runtime_error("VIDIOC_DQBUF error ("+std::string(strerror(errno))+")");
					}


			}
		} else{
			//ошибки нет.
			if (buf.index > n_buffers - 1)
				throw std::runtime_error("VIDIOC_DQBUF return invalid buffer index");

			buffer* b = &buffers[buf.index];

			//printf("dqbuf:ret:%d,buf.index:%d,buf.bytesused:%d,start:%08X\n",ret,buf.index,buf.bytesused,(int)buffers[buf.index].start);

			f3ptr = frame_ptr(new frame());
			f3ptr->tp = boost::chrono::steady_clock::now();
			f3ptr->dcb = boost::bind(&capturer::return_frame,this,f3ptr->tp,b);

			//construct avframe
			//std::cout<<"construct avframe (buf:"<<b->index<<")...";
			if (selected_format.need_codec_id==AV_CODEC_ID_MJPEG){

			} else{
				f3ptr->avframe = libav::av_frame_alloc();
				int _size = libav::avpicture_fill((AVPicture *)f3ptr->avframe, (uint8_t*)b->start, selected_format.ffmpeg_pixfmt, 
					current_frame_size.width, current_frame_size.height);
				f3ptr->avframe->width = current_frame_size.width;
				f3ptr->avframe->height = current_frame_size.height;
				f3ptr->avframe->format = int(selected_format.ffmpeg_pixfmt);
			}
			//std::cout<<"ok"<<std::endl;
			b->extracted = true;

			dequeued_buffers_count++;



		}
	}
#endif

	return f3ptr;
}

bool camera_v4l2::wait_all_extracted_buffers(boost::chrono::steady_clock::duration max_duration){
	//wait all buffers to return



	namespace bc = boost::chrono;
	bc::steady_clock::time_point start_tp = bc::steady_clock::now();
	int extracted_buffers_count = 0xFF;
	while (extracted_buffers_count > 0){

		boost::unique_lock<boost::recursive_mutex> l1(internal_mutex);

		extracted_buffers_count = 0;
		if (n_buffers>0)
			for (int i=0;i!=n_buffers;i++)
				if (buffers[i].extracted)
					extracted_buffers_count ++;
		if (extracted_buffers_count==0)
			break ;

		if (max_duration != boost::chrono::steady_clock::duration())
			if (bc::steady_clock::duration(bc::steady_clock::now() - start_tp) > max_duration)
				break;

		l1.unlock();

		boost::this_thread::sleep(boost::posix_time::milliseconds(50));
	}

	if (extracted_buffers_count==0)
		std::cout<<"all buffers returned"<<std::endl;

	return extracted_buffers_count==0;

}






std::string v4l2_pisxftm_fourcc_to_str2(unsigned int v4l2_pixfmt){
	char fourcc_ch[5];
	memset(fourcc_ch,0,5);
	memcpy(fourcc_ch,&v4l2_pixfmt,4);
	return std::string(fourcc_ch);
}


void camera_v4l2::do_stop_streaming(){


	
	std::cout<<"enter to DoStopStreaming()"<<std::endl;
	if (streaming_state == vs_streaming){

		streaming_state = vs_initialization;


		boost::unique_lock<boost::recursive_mutex> l1(internal_mutex);
		std::cout<<"internal_mutex locked"<<std::endl;
		last_frame = frame_ptr();
		l1.unlock();

		if (!wait_all_extracted_buffers(boost::chrono::seconds(1000)))
			throw std::runtime_error("cannot wait extracted buffers");

		std::cout<<"all extracted buffers returned"<<std::endl;

		l1.lock();
		std::string error_message;
		if (!do_stop_streaming_v4l2(fd,error_message)){
			using namespace Utility;
			Journal::Instance()->Write(ERR,DST_STDERR|DST_SYSLOG,"streaming stop error:%s",error_message.c_str());
		}

		unmap_buffers();
		current_frame_size = frame_size();

		streaming_state = vs_stopped;
	}
	std::cout<<"DoStopStreaming() done"<<std::endl;

}

capturer::definition camera_v4l2::do_get_definition() const{
	return v4l2_device_definition;
}
capturer::capabilities camera_v4l2::do_get_capabilities() const{
	return v4l2_device_capabilities;
}
