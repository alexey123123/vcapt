#include <iostream>

#include "openmax_il_helper.h"


#if defined(OPENMAX_IL)
#pragma pack(4)
#include <ilclient/ilclient.h>
#include <bcm_host.h>
#pragma pack()
#endif

class __OMX_initializer__{
public:
	__OMX_initializer__(){
#if defined(OPENMAX_IL)
		bcm_host_init();
		OMX_Deinit();
		OMX_Init();
		std::cout<<"--OpenMax initialized--"<<std::endl;
#endif
	}
	~__OMX_initializer__(){
#if defined(OPENMAX_IL)
		OMX_Deinit();
		bcm_host_deinit();
		std::cout<<"--OpenMax deinitialized--"<<std::endl;
#endif
	}

};

static __OMX_initializer__ __i__;


#define VIDEO_ENCODE_PORT_IN 200
#define VIDEO_ENCODE_PORT_OUT 201
#define JPEG_ENCODE_PORT_IN 340
#define JPEG_ENCODE_PORT_OUT 341
#define RESIZE_PORT_IN 60
#define RESIZE_PORT_OUT 61

class openmax_il_helper::Impl{
public:
	Impl(){
#if defined(OPENMAX_IL)
		video_encode = 0;
		
		
		resizer = 0;
		last_src_pixfmt = AV_PIX_FMT_NONE;
		last_src_width = 0;
		last_src_height = 0;
		last_dst_pixfmt = AV_PIX_FMT_NONE;
		last_dst_width = 0;
		last_dst_height = 0;



		client = ilclient_init();
#endif
	};

	~Impl(){
#if defined(OPENMAX_IL)
		
		COMPONENT_T* comps[255];
		memset(comps,0,sizeof(comps));
		int i = 0;
		if (video_encode){
			ilclient_change_component_state(video_encode, OMX_StateIdle);

			ilclient_disable_port_buffers(video_encode, VIDEO_ENCODE_PORT_IN, NULL, NULL, NULL);
			ilclient_disable_port_buffers(video_encode, VIDEO_ENCODE_PORT_OUT, NULL, NULL, NULL);

			comps[i] = video_encode;
			i++;
		}

		ilclient_cleanup_components(comps);


		if (client)
			ilclient_destroy(client);

#endif
		
		
	}

	void init_h264_encoder(int width,int height,int bitrate){
		std::cout << "OpenMAX h264 encoder init started" << std::endl;
#if defined(OPENMAX_IL)
		#pragma pack(4)

		if (video_encode){
			ilclient_change_component_state(video_encode, OMX_StateIdle);

			ilclient_disable_port_buffers(video_encode, VIDEO_ENCODE_PORT_IN, NULL, NULL, NULL);
			ilclient_disable_port_buffers(video_encode, VIDEO_ENCODE_PORT_OUT, NULL, NULL, NULL);
			disable_single_component(video_encode);
		}
		


		OMX_VIDEO_PARAM_PORTFORMATTYPE format;
		OMX_PARAM_PORTDEFINITIONTYPE def;

		int r = 0;

		ilclient_create_component(client, &video_encode, "video_encode", 
			(ILCLIENT_CREATE_FLAGS_T)(ILCLIENT_DISABLE_ALL_PORTS | 
			ILCLIENT_ENABLE_INPUT_BUFFERS | 
			ILCLIENT_ENABLE_OUTPUT_BUFFERS));


		memset(&def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
		def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
		def.nVersion.nVersion = OMX_VERSION;
		def.nPortIndex = VIDEO_ENCODE_PORT_IN;

		OMX_GetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexParamPortDefinition, &def);

		def.format.video.nFrameWidth = width;
		def.format.video.nFrameHeight = height;
		int FR = 25;
		
		switch(width){
			case 1280:
				FR = 14;
				break;

		}
		printf("FR = %d\n",FR);
		def.format.video.xFramerate = FR << 16;
		
		
		def.format.video.nSliceHeight = def.format.video.nFrameHeight;
		def.format.video.nStride = def.format.video.nFrameWidth;
		def.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;

		r = OMX_SetParameter(ILC_GET_HANDLE(video_encode),
			OMX_IndexParamPortDefinition, 
			&def);


		memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
		format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
		format.nVersion.nVersion = OMX_VERSION;
		format.nPortIndex = VIDEO_ENCODE_PORT_OUT;
		format.eCompressionFormat = OMX_VIDEO_CodingAVC;

		r = OMX_SetParameter(ILC_GET_HANDLE(video_encode),
			OMX_IndexParamVideoPortFormat, 
			&format);



		OMX_VIDEO_PARAM_BITRATETYPE bitrateType;
		memset(&bitrateType, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
		bitrateType.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
		bitrateType.nVersion.nVersion = OMX_VERSION;
		bitrateType.eControlRate = OMX_Video_ControlRateVariable;
		bitrateType.nTargetBitrate = bitrate;
		bitrateType.nPortIndex = VIDEO_ENCODE_PORT_OUT;
		r = OMX_SetParameter(ILC_GET_HANDLE(video_encode),
			OMX_IndexParamVideoBitrate, &bitrateType);


		
		
		OMX_VIDEO_PARAM_PROFILELEVELTYPE s_prof;
		memset(&s_prof, 0, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
		s_prof.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
		s_prof.nVersion.nVersion = OMX_VERSION;
		s_prof.nPortIndex = VIDEO_ENCODE_PORT_OUT;
		s_prof.eProfile = OMX_VIDEO_AVCProfileBaseline;
		s_prof.eLevel= OMX_VIDEO_AVCLevel41;

		r = OMX_SetParameter(ILC_GET_HANDLE(video_encode),
			OMX_IndexParamVideoProfileLevelCurrent, &s_prof);

		
		
		
		OMX_VIDEO_CONFIG_AVCINTRAPERIOD i_period;
		i_period.nSize = sizeof(OMX_VIDEO_CONFIG_AVCINTRAPERIOD);
		i_period.nVersion.nVersion = OMX_VERSION;
		i_period.nPortIndex = VIDEO_ENCODE_PORT_OUT;
		OMX_GetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexConfigVideoAVCIntraPeriod, &i_period);
		std::cout<<i_period.nIDRPeriod<<" "<<i_period.nPFrames<<std::endl;
		i_period.nIDRPeriod = 11;
		i_period.nPFrames = 11;
		OMX_SetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexConfigVideoAVCIntraPeriod, &i_period);
		OMX_GetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexConfigVideoAVCIntraPeriod, &i_period);
		std::cout<<"After set:"<<i_period.nIDRPeriod<<" "<<i_period.nPFrames<<std::endl;
		
		/*
		OMX_PARAM_U32TYPE omx_u32;
		omx_u32.nSize = sizeof(OMX_PARAM_U32TYPE);
		omx_u32.nVersion.nVersion = OMX_VERSION;
		omx_u32.nPortIndex = VIDEO_ENCODE_PORT_OUT;
		OMX_GetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexConfigBrcmVideoIntraPeriodTime, &omx_u32);
		std::cout<<"OMX_IndexConfigBrcmVideoIntraPeriodTime="<<omx_u32.nU32<<std::endl;

		omx_u32.nSize = sizeof(OMX_PARAM_U32TYPE);
		omx_u32.nVersion.nVersion = OMX_VERSION;
		omx_u32.nPortIndex = VIDEO_ENCODE_PORT_OUT;
		OMX_GetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexConfigBrcmVideoIntraPeriod, &omx_u32);
		std::cout<<"OMX_IndexConfigBrcmVideoIntraPeriod="<<omx_u32.nU32<<std::endl;

		omx_u32.nU32 = 11;
		OMX_SetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexConfigBrcmVideoIntraPeriod, &omx_u32);
		OMX_GetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexConfigBrcmVideoIntraPeriod, &omx_u32);
		std::cout<<"OMX_IndexConfigBrcmVideoIntraPeriod (after)="<<omx_u32.nU32<<std::endl;


		*/


		ilclient_change_component_state(video_encode, OMX_StateIdle);

		ilclient_enable_port_buffers(video_encode, VIDEO_ENCODE_PORT_IN, NULL, NULL, NULL);

		ilclient_enable_port_buffers(video_encode, VIDEO_ENCODE_PORT_OUT, NULL, NULL, NULL);

		ilclient_change_component_state(video_encode, OMX_StateExecuting);


		std::cout << "OpenMAX h264 encoder Ok ("<<width<<","<<height<<","<<bitrate<<")" << std::endl;
		bf2_size = 0;
		#pragma pack()
#endif

	}

	void get_header_packets(std::deque<packet_ptr>& packets){
		if (start_packet_data.size() > 0){
			packet_ptr p(new packet());
			libav::av_new_packet(&p->p,start_packet_data.size());
			memcpy(p->p.data,
				&start_packet_data[0],p->p.size);
			p->p.flags = AV_PKT_FLAG_KEY;


			packets.push_back(p);
		}
	}

	bool encode_frame(AVFrame* input_frame,AVPacket& output_packet){
#if defined(OPENMAX_IL)
		OMX_BUFFERHEADERTYPE *buf; //входной буфер
		OMX_BUFFERHEADERTYPE *out; //выходной буфер


		buf = ilclient_get_input_buffer(video_encode, VIDEO_ENCODE_PORT_IN, 1);
		buf->nFilledLen = libav::avpicture_layout((AVPicture *)input_frame,PIX_FMT_YUV420P,
			input_frame->width,input_frame->height,buf->pBuffer,buf->nAllocLen);

		if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_encode), buf) !=
			OMX_ErrorNone) {
				printf("Error emptying buffer!\n");
		}


		out = ilclient_get_output_buffer(video_encode, VIDEO_ENCODE_PORT_OUT, 1);
		int r = OMX_FillThisBuffer(ILC_GET_HANDLE(video_encode), out);
		if (r != OMX_ErrorNone) {
			printf("Error filling buffer: %x\n", r);
		}


		if (out->nFilledLen==0)
			return false;


		if (out->nFlags & OMX_BUFFERFLAG_CODECCONFIG){
			std::copy(out->pBuffer + out->nOffset, out->pBuffer + out->nOffset + out->nFilledLen, std::back_inserter(temp_data));
			if (out->nFlags & OMX_BUFFERFLAG_ENDOFFRAME){
				start_packet_data = temp_data;
				temp_data.clear();
			}			
		}

		memcpy(bf2 + bf2_size, out->pBuffer + out->nOffset, out->nFilledLen);
		bf2_size += out->nFilledLen;

		if ((out->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) == 0){
			return false;
		}

		//output_packet.data = bf2;
		//output_packet.size = bf2_size;
		libav::av_new_packet(&output_packet,bf2_size);
		memcpy(output_packet.data,bf2,bf2_size);


		bf2_size = 0;




		output_packet.flags = 0;
		//if (out->nFlags & OMX_BUFFERFLAG_SYNCFRAME)
		output_packet.flags |= AV_PKT_FLAG_KEY; //stream correctly plays in vlc only if every frame is KEY.

#endif

		return true;

	}

	void init_resizer(
				AVPixelFormat f_scr, int src_width,int src_height,
				AVPixelFormat f_dst, int dst_width,int dst_height){

		//init hardware
#if defined(OPENMAX_IL)
#pragma pack(4)
		
		if (!resizer){
			ilclient_create_component(client, &resizer, "resize", 
				(ILCLIENT_CREATE_FLAGS_T)(ILCLIENT_DISABLE_ALL_PORTS | 
				ILCLIENT_ENABLE_INPUT_BUFFERS | 
				ILCLIENT_ENABLE_OUTPUT_BUFFERS));

			if (!resizer)
				throw std::runtime_error("OpenMAX: cannot initialize component");
		}

		ilclient_change_component_state(resizer, OMX_StateIdle);

		ilclient_disable_port_buffers(resizer, RESIZE_PORT_IN, NULL, NULL, NULL);
		ilclient_disable_port_buffers(resizer, RESIZE_PORT_OUT, NULL, NULL, NULL);		


		OMX_VIDEO_PARAM_PORTFORMATTYPE format;
		OMX_PARAM_PORTDEFINITIONTYPE def;

		int r = 0;



		memset(&def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
		def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
		def.nVersion.nVersion = OMX_VERSION;
		def.nPortIndex = RESIZE_PORT_IN;

		OMX_GetParameter(ILC_GET_HANDLE(resizer), OMX_IndexParamPortDefinition, &def);

		def.format.image.nFrameWidth = src_width;
		def.format.image.nFrameHeight = src_height;
		def.format.image.nSliceHeight = def.format.video.nFrameHeight;
		def.format.image.nStride = def.format.video.nFrameWidth;
		def.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
		def.format.image.pNativeWindow = 0;
		switch(f_scr){
			case PIX_FMT_YUV420P:
				def.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
				break;
			case PIX_FMT_RGB565:
				def.format.image.eColorFormat = OMX_COLOR_Format16bitRGB565;
				break;
			default:
				throw std::runtime_error("pixel format not supported");
		}	

		r = OMX_SetParameter(ILC_GET_HANDLE(resizer),
			OMX_IndexParamPortDefinition, 
			&def);



		// set input image format
		OMX_IMAGE_PARAM_PORTFORMATTYPE imagePortFormat;
		memset(&imagePortFormat, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
		imagePortFormat.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
		imagePortFormat.nVersion.nVersion = OMX_VERSION;
		imagePortFormat.nPortIndex = RESIZE_PORT_IN;
		imagePortFormat.eCompressionFormat = OMX_IMAGE_CodingUnused;
		switch(f_scr){
		case PIX_FMT_YUV420P:
			imagePortFormat.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
			break;
		case PIX_FMT_RGB565:
			imagePortFormat.eColorFormat = OMX_COLOR_Format16bitRGB565;
			break;
		default:
			throw std::runtime_error("pixel format not supported");
		}	
		OMX_SetParameter(ILC_GET_HANDLE(resizer),
			OMX_IndexParamImagePortFormat, &imagePortFormat);




		memset(&def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
		def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
		def.nVersion.nVersion = OMX_VERSION;
		def.nPortIndex = RESIZE_PORT_OUT;

		OMX_GetParameter(ILC_GET_HANDLE(resizer), OMX_IndexParamPortDefinition, &def);

		def.format.image.nFrameWidth = dst_width;
		def.format.image.nFrameHeight = dst_height;
		def.format.image.nSliceHeight = def.format.video.nFrameHeight;
		def.format.image.nStride = def.format.video.nFrameWidth;
		def.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
		def.format.image.pNativeWindow = 0;
		switch(f_dst){
		case PIX_FMT_YUV420P:
			def.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
			break;
		case PIX_FMT_RGB565:
			def.format.image.eColorFormat = OMX_COLOR_Format16bitRGB565;
			break;
		default:
			throw std::runtime_error("pixel format not supported");
		}


		r = OMX_SetParameter(ILC_GET_HANDLE(resizer),
			OMX_IndexParamPortDefinition, 
			&def);

		// set input image format
		memset(&imagePortFormat, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
		imagePortFormat.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
		imagePortFormat.nVersion.nVersion = OMX_VERSION;
		imagePortFormat.nPortIndex = RESIZE_PORT_OUT;
		imagePortFormat.eCompressionFormat = OMX_IMAGE_CodingUnused;
		switch(f_scr){
		case PIX_FMT_YUV420P:
			imagePortFormat.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
			break;
		case PIX_FMT_RGB565:
			imagePortFormat.eColorFormat = OMX_COLOR_Format16bitRGB565;
			break;
		default:
			throw std::runtime_error("pixel format not supported");
		}	
		OMX_SetParameter(ILC_GET_HANDLE(resizer),
			OMX_IndexParamImagePortFormat, &imagePortFormat);





		//----------------------------------------

		ilclient_change_component_state(resizer, OMX_StateIdle);

		ilclient_enable_port_buffers(resizer, RESIZE_PORT_IN, NULL, NULL, NULL);

		ilclient_enable_port_buffers(resizer, RESIZE_PORT_OUT, NULL, NULL, NULL);

		ilclient_change_component_state(resizer, OMX_StateExecuting);


		std::cout << "OpenMAX resizer Ok ("<<src_width<<":"<<src_height<<" -> "<<dst_width<<":"<<dst_height<<")" << std::endl;
		bf2_size = 0;
#pragma pack()
#endif

	}
	frame_ptr resize_frame(AVFrame* input_frame, AVPixelFormat dst_pixfmt, int dst_width, int dst_height){

		if (input_frame->format != dst_pixfmt)
			throw std::runtime_error("pixel format conversation not supported");
		if ((input_frame->format != PIX_FMT_YUV420P)&&(input_frame->format != PIX_FMT_RGB565))
			throw std::runtime_error("pixel format not supported");

		if ((last_src_pixfmt != input_frame->format)||
			(last_src_width != input_frame->width)||
			(last_src_height != input_frame->height)||
			(last_dst_pixfmt != dst_pixfmt)||
			(last_dst_width != dst_width)||
			(last_dst_height != dst_height)){

				init_resizer(dst_pixfmt,input_frame->width,input_frame->height,dst_pixfmt,dst_width,dst_height);

				last_src_pixfmt = (AVPixelFormat)input_frame->format;
				last_src_width = input_frame->width;
				last_src_height = input_frame->height;
				last_dst_pixfmt = dst_pixfmt;
				last_dst_width = dst_width;
				last_dst_height = dst_height;
		}
			



		frame_ptr ret_f;
#if defined(OPENMAX_IL)
		OMX_BUFFERHEADERTYPE *buf; //входной буфер
		OMX_BUFFERHEADERTYPE *out; //выходной буфер


		buf = ilclient_get_input_buffer(resizer, RESIZE_PORT_IN, 1);
		buf->nFilledLen = libav::avpicture_layout((AVPicture *)input_frame,PIX_FMT_YUV420P,
			input_frame->width,input_frame->height,buf->pBuffer,buf->nAllocLen);

		//std::cout<<"in_buf->nFilledLen="<<buf->nFilledLen<<std::endl;

		if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(resizer), buf) != OMX_ErrorNone)
			throw std::runtime_error("OpenMax error: resizer: error emptying IN buffer");


		out = ilclient_get_output_buffer(resizer, RESIZE_PORT_OUT, 1);
		int r = OMX_FillThisBuffer(ILC_GET_HANDLE(resizer), out);
		if (r != OMX_ErrorNone) 
			throw std::runtime_error("OpenMax error: resizer: error filling OUT buffer");


		if (out->nFilledLen==0)
			throw std::runtime_error("OpenMax error: resizer: OUT buffer nFilledLen==0");

		std::cout<<"out_buf->nFilledLen="<<out->nFilledLen<<std::endl;

		ret_f = frame_ptr(new frame());
		ret_f->frame_data = boost::shared_array<unsigned char>(new unsigned char [out->nFilledLen]);
		memcpy(ret_f->frame_data.get(),out->pBuffer,out->nFilledLen);

		ret_f->avframe = libav::avcodec_alloc_frame();
		libav::avpicture_fill((AVPicture*)ret_f->avframe,
			ret_f->frame_data.get(),
			dst_pixfmt,input_frame->width,input_frame->height);

#endif

		return ret_f;


	}

private:
	
#if defined(OPENMAX_IL)
	ILCLIENT_T *client;

	COMPONENT_T *video_encode;
	COMPONENT_T *resizer;

	void disable_single_component(COMPONENT_T *c){
		COMPONENT_T* comps[2];
		memset(comps,0,sizeof(comps));
		comps[0] = c;
		ilclient_cleanup_components(comps);
	}
#endif

	//RPi hw-encoder returns config-data in first requests
	std::vector<unsigned char> start_packet_data;
	std::vector<unsigned char> temp_data;

	std::vector<unsigned char> frame_part_buffer;

	unsigned char bf2[100000];
	int bf2_size;

	//resizer stuff
	AVPixelFormat last_src_pixfmt;
	int last_src_width;
	int last_src_height;
	AVPixelFormat last_dst_pixfmt;
	int last_dst_width;
	int last_dst_height;

	
};


openmax_il_helper::openmax_il_helper(){
	pimpl = new Impl();
}

openmax_il_helper::~openmax_il_helper(){
	delete pimpl;
}

void openmax_il_helper::init_h264_encoder(int width,int height,int bitrate){
	pimpl->init_h264_encoder(width,height,bitrate);
}

void openmax_il_helper::get_header_packets(std::deque<packet_ptr>& packets){
	pimpl->get_header_packets(packets);
}
bool openmax_il_helper::encode_frame(AVFrame* input_frame,AVPacket& output_packet){
	return pimpl->encode_frame(input_frame,output_packet);
}


frame_ptr openmax_il_helper::resize_frame(AVFrame* input_frame, AVPixelFormat dst_pixfmt, int dst_width, int dst_height){
	return pimpl->resize_frame(input_frame,dst_pixfmt,dst_width,dst_height);
}

