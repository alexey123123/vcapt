#include <iostream>

#if defined(RPI)
#include <ilclient/ilclient.h>
#include <bcm_host.h>
#endif

#include "rpi_openmax.h"
#include "libav.h"
#include "ffmpeg_encoder.h"

static int rpi_hw_encoder_inst_count = 0;

class rpi_openmax::Impl{
public:
	Impl(){
		if (rpi_hw_encoder_inst_count!=0)
			throw ffmpeg_encoder::hw_encoder_busy_exception();

		rpi_hw_encoder_inst_count++;
	}
	~Impl(){
		rpi_hw_encoder_inst_count--;
		//TODO: uninit
#if defined(RPI)
		ilclient_disable_port_buffers(video_encode, 200, NULL, NULL, NULL);
		ilclient_disable_port_buffers(video_encode, 201, NULL, NULL, NULL);

// 		ilclient_state_transition(list, OMX_StateIdle);
// 		ilclient_state_transition(list, OMX_StateLoaded);
// 
// 		ilclient_cleanup_components(list);

		OMX_Deinit();

		ilclient_destroy(client);

#endif//RPI
		
	}

	bool initialize(int width,int height,int bitrate){
#if defined(RPI)

		OMX_VIDEO_PARAM_PORTFORMATTYPE format;
		OMX_PARAM_PORTDEFINITIONTYPE def;

		int r = 0;

#define VIDEO_ENCODE_PORT_IN 200
#define VIDEO_ENCODE_PORT_OUT 201

#define FPS 1000

		bcm_host_init();

		client = ilclient_init();
		OMX_Init();
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
		def.format.video.xFramerate = FPS << 16;
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


		ilclient_change_component_state(video_encode, OMX_StateIdle);

		ilclient_enable_port_buffers(video_encode, VIDEO_ENCODE_PORT_IN, NULL, NULL, NULL);

		ilclient_enable_port_buffers(video_encode, VIDEO_ENCODE_PORT_OUT, NULL, NULL, NULL);

		ilclient_change_component_state(video_encode, OMX_StateExecuting);


		std::cout << "RPi encoder Ok" << std::endl;
#endif
		return true;
	}


	bool EncodeFrame(AVFrame* input_frame,AVPacket& output_packet){
#if defined(RPI)

		OMX_BUFFERHEADERTYPE *buf; //входной буфер
		OMX_BUFFERHEADERTYPE *out; //выходной буфер

		buf = ilclient_get_input_buffer(video_encode, 200, 1);
		buf->nFilledLen = libav::avpicture_layout((AVPicture *)input_frame,PIX_FMT_YUV420P,
			input_frame->width,input_frame->height,buf->pBuffer,buf->nAllocLen);

		if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_encode), buf) !=
			OMX_ErrorNone) {
				printf("Error emptying buffer!\n");
		}


		out = ilclient_get_output_buffer(video_encode, VIDEO_ENCODE_PORT_OUT, 1);
		OMX_FillThisBuffer(ILC_GET_HANDLE(video_encode), out);

		output_packet.data= out->pBuffer;
		output_packet.size= out->nFilledLen;
		output_packet.flags |= AV_PKT_FLAG_KEY;

		

		/*
		AVRational omxtimebase = { 1, FPS};
		AVRational stream_timebase = { 1, 1000};


		output_packet.pts = libav::av_rescale_q(input_frame->pts, 
			omxtimebase,
			stream_timebase);

		output_packet.dts = output_packet.pts;
		output_packet.duration = 0;
		*/
#endif

		return true;
	}


#if defined(RPI)
	COMPONENT_T *video_encode;
	ILCLIENT_T *client;
#endif
	private:
		int framenumber;

};




rpi_openmax::rpi_openmax(){
	pimpl = new Impl();
}
bool rpi_openmax::initialize(int width,int height,int bitrate){
	return pimpl->initialize(width,height,bitrate);
}


bool rpi_openmax::encode_frame(AVFrame* input_frame,AVPacket& output_packet){
	return pimpl->EncodeFrame(input_frame,output_packet);
}
rpi_openmax::~rpi_openmax(){
	delete pimpl;
}

