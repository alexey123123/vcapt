#include "codec_rpi.h"

#if defined(RPI)
#pragma pack(4)
#include <ilclient/ilclient.h>
#include <bcm_host.h>
#pragma pack()
#endif

static int rpi_hw_encoder_inst_count = 0;


#define VIDEO_ENCODE_PORT_IN 200
#define VIDEO_ENCODE_PORT_OUT 201
#define JPEG_ENCODE_PORT_IN 340
#define JPEG_ENCODE_PORT_OUT 341


class codec_rpi::Impl{
public:
	Impl():entire_frame_returned(false){
// 		if (rpi_hw_encoder_inst_count!=0)
// 			throw std::runtime_error("RPI harware encoder overloaded");

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

	bool initialize_h264(int width,int height,int bitrate){
#if defined(RPI)
		#pragma pack(4)

		OMX_VIDEO_PARAM_PORTFORMATTYPE format;
		OMX_PARAM_PORTDEFINITIONTYPE def;

		int r = 0;

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
		def.format.video.xFramerate = 25 << 16;
		//def.format.video.xFramerate = 0;
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
		s_prof.eLevel= OMX_VIDEO_AVCLevel4;

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


		std::cout << "RPi AVC encoder Ok ("<<width<<","<<height<<","<<bitrate<<")" << std::endl;
		#pragma pack()
#endif
		return true;
	}




	bool EncodeFrame(AVFrame* input_frame,AVPacket& output_packet){
#if defined(RPI)
		#pragma pack(4)
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

		if (entire_frame_returned){
			entire_frame_returned = false;
			frame_part_buffer.clear();
		}

		if ((out->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) == 0){
			//copy data to temp buffer
			std::copy(out->pBuffer,out->pBuffer + out->nFilledLen, std::back_inserter(frame_part_buffer));
			std::cout<<"buffered:"<<out->nFilledLen<<" bytes"<<std::endl;

			return false;
		}

		if (frame_part_buffer.size() != 0){
			std::copy(out->pBuffer,out->pBuffer + out->nFilledLen, std::back_inserter(frame_part_buffer));
			std::cout<<"buffered:"<<out->nFilledLen<<" bytes"<<std::endl;

			output_packet.data= &frame_part_buffer[0];
			output_packet.size= frame_part_buffer.size();

			entire_frame_returned = true;
		} else{
			output_packet.data= out->pBuffer + out->nOffset;
			output_packet.size= out->nFilledLen;
		}



		
		output_packet.flags = 0;
// 		if (out->nFlags & OMX_BUFFERFLAG_SYNCFRAME)
// 			output_packet.flags |= AV_PKT_FLAG_KEY;
		output_packet.flags |= AV_PKT_FLAG_KEY; //stream correctly plays in vlc only if every frame is KEY.

#pragma pack()
#endif

		return true;
	}

	std::vector<unsigned char> frame_part_buffer;
	bool entire_frame_returned;


#if defined(RPI)
	COMPONENT_T *video_encode;
	ILCLIENT_T *client;
#endif
	private:
		int framenumber;

};


codec_rpi::codec_rpi():codec_libav(){
	pimpl = new Impl();
}
codec_rpi::~codec_rpi(){
	delete pimpl;
}

packet_ptr codec_rpi::do_process_frame(capturer::frame_ptr fptr){

	packet_ptr ret_packet(new packet());
	ret_packet->frame_tp = fptr->tp;

	using namespace boost::chrono;

	libav::av_init_packet(&ret_packet->p);
	ret_packet->p.data = 0;
	ret_packet->p.size = 0;
	ret_packet->p.pts = 0;
	ret_packet->p.dts = 0;
	ret_packet->p.duration = 0;


	fptr->avframe->pts = boost::chrono::duration_cast<boost::chrono::milliseconds>(fptr->tp - codec_start_tp).count();

	if (!pimpl->EncodeFrame(fptr->avframe,ret_packet->p))
		ret_packet = packet_ptr();

	return ret_packet;
}

void codec_rpi::do_initilalize(const format& f, AVPixelFormat& _codec_pixfmt){

	if ((f.name != "h264"))
		throw std::runtime_error("RPI supports only h264 formats");


	codec_libav::do_initilalize(f,_codec_pixfmt);//	need a fake libav codec initialization, because we need AVCodecContext
												 // for streaming h264 throw livav containers...		


	//now init hardware
	if (f.name=="h264")
		if (!pimpl->initialize_h264(f.fsize.width,f.fsize.height,f.bitrate))
			throw std::runtime_error("RPI hardware error");
//	TODO:
// 	if (f.name=="mjpeg")
// 		if (!pimpl->initialize_jpeg(f.fsize.width,f.fsize.height))
// 			throw std::runtime_error("RPI hardware error");

}


/*

int initialize_jpeg2(int width,int height){
#if defined(RPI)


OMX_HANDLETYPE  handle;
int inPort,outPort;
int inputBufferHeaderCount;
OMX_BUFFERHEADERTYPE **ppInputBufferHeader;

#define TIMEOUT_MS 2000


bcm_host_init();

client = ilclient_init();
OMX_Init();
int             ret = ilclient_create_component(client,
&video_encode,
"image_encode",
(ILCLIENT_CREATE_FLAGS_T)(
ILCLIENT_DISABLE_ALL_PORTS
|
ILCLIENT_ENABLE_INPUT_BUFFERS));

if (ret != 0) {
printf("image encpde\n");
return -1;
}
// grab the handle for later use in OMX calls directly
handle =
ILC_GET_HANDLE(video_encode);

// get and store the ports
OMX_PORT_PARAM_TYPE port;
port.nSize = sizeof(OMX_PORT_PARAM_TYPE);
port.nVersion.nVersion = OMX_VERSION;

OMX_GetParameter(handle,
OMX_IndexParamImageInit, &port);
if (port.nPorts != 2) {
return -1;
}
inPort = port.nStartPortNumber;
outPort = port.nStartPortNumber + 1;


// move to idle
ilclient_change_component_state(video_encode,OMX_StateIdle);

// set input image format
OMX_IMAGE_PARAM_PORTFORMATTYPE imagePortFormat;
memset(&imagePortFormat, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
imagePortFormat.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
imagePortFormat.nVersion.nVersion = OMX_VERSION;
imagePortFormat.nPortIndex = inPort;
imagePortFormat.eCompressionFormat = OMX_IMAGE_CodingUnused;
imagePortFormat.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
OMX_SetParameter(video_encode, OMX_IndexParamImagePortFormat, &imagePortFormat);

// get buffer requirements
OMX_PARAM_PORTDEFINITIONTYPE portdef;
portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
portdef.nVersion.nVersion = OMX_VERSION;
portdef.nPortIndex = inPort;
OMX_GetParameter(handle,OMX_IndexParamPortDefinition, &portdef);

// enable the port and setup the buffers
OMX_SendCommand(handle,OMX_CommandPortEnable,inPort, NULL);
inputBufferHeaderCount = portdef.nBufferCountActual;
// allocate pointer array
ppInputBufferHeader = (OMX_BUFFERHEADERTYPE **) malloc(sizeof(void) * inputBufferHeaderCount);
// allocate each buffer
int i;
for (i = 0; i < inputBufferHeaderCount; i++) {
if (OMX_AllocateBuffer(handle,&ppInputBufferHeader[i],inPort,(void *)i,
portdef.nBufferSize) != OMX_ErrorNone) {
printf("Allocate decode buffer\n");
return -1;
}
}
// wait for port enable to complete - which it should once buffers are 
// assigned
ret =
ilclient_wait_for_event(video_encode,
OMX_EventCmdComplete,
OMX_CommandPortEnable, 0,
inPort, 0,
0, TIMEOUT_MS);
if (ret != 0) {
printf("Did not get port enable %d\n", ret);
return -1;
}



OMX_IMAGE_PARAM_PORTFORMATTYPE format;
memset(&format, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
format.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
format.nVersion.nVersion = OMX_VERSION;
format.nPortIndex = outPort;
format.eCompressionFormat = OMX_IMAGE_CodingJPEG;

OMX_SetParameter(handle,
OMX_IndexParamImagePortFormat, 
&format);


OMX_IMAGE_PARAM_QFACTORTYPE qfactor_type;
memset(&qfactor_type, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
qfactor_type.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
qfactor_type.nVersion.nVersion = OMX_VERSION;
qfactor_type.nPortIndex = outPort;
qfactor_type.nQFactor = 75;
OMX_SetParameter(handle,
OMX_IndexParamQFactor, &qfactor_type);


// start executing the decoder 
ret = OMX_SendCommand(handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
if (ret != 0) {
printf("Error starting image decoder %x\n", ret);
return -1;
}
ret = ilclient_wait_for_event(video_encode,
OMX_EventCmdComplete,
OMX_StateExecuting, 0, 0, 1, 0,
TIMEOUT_MS);
if (ret != 0) {
printf("Did not receive executing stat %d\n", ret);
// return OMXJPEG_ERROR_EXECUTING;
}
#endif
return 0;

}



bool initialize_jpeg(int width,int height){
#if defined(RPI)



int r = 0;


#define FPS 1000

bcm_host_init();

client = ilclient_init();
OMX_Init();
ilclient_create_component(client, &video_encode, "image_encode",
(ILCLIENT_CREATE_FLAGS_T)(ILCLIENT_DISABLE_ALL_PORTS | 
ILCLIENT_ENABLE_INPUT_BUFFERS | 
ILCLIENT_ENABLE_OUTPUT_BUFFERS));

OMX_PARAM_PORTDEFINITIONTYPE def;
memset(&def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
def.nVersion.nVersion = OMX_VERSION;
def.nPortIndex = JPEG_ENCODE_PORT_IN;

OMX_GetParameter(ILC_GET_HANDLE(video_encode), OMX_IndexParamPortDefinition, &def);

//def.format.image.cMIMEType = "image/jpeg";
def.format.image.nFrameWidth = width;
def.format.image.nFrameHeight = height;
def.format.image.nSliceHeight = def.format.image.nFrameHeight;
def.format.image.nStride = def.format.image.nFrameWidth;
def.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;

r = OMX_SetParameter(ILC_GET_HANDLE(video_encode),
OMX_IndexParamPortDefinition, 
&def);



OMX_IMAGE_PARAM_PORTFORMATTYPE format;
memset(&format, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
format.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
format.nVersion.nVersion = OMX_VERSION;
format.nPortIndex = JPEG_ENCODE_PORT_OUT;
format.eCompressionFormat = OMX_IMAGE_CodingJPEG;

r = OMX_SetParameter(ILC_GET_HANDLE(video_encode),
OMX_IndexParamImagePortFormat, 
&format);


OMX_IMAGE_PARAM_QFACTORTYPE qfactor_type;
memset(&qfactor_type, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
qfactor_type.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
qfactor_type.nVersion.nVersion = OMX_VERSION;
qfactor_type.nPortIndex = JPEG_ENCODE_PORT_OUT;
qfactor_type.nQFactor = 75;
r = OMX_SetParameter(ILC_GET_HANDLE(video_encode),
OMX_IndexParamQFactor, &qfactor_type);


ilclient_change_component_state(video_encode, OMX_StateIdle);

ilclient_enable_port_buffers(video_encode, JPEG_ENCODE_PORT_IN, NULL, NULL, NULL);

ilclient_enable_port_buffers(video_encode, JPEG_ENCODE_PORT_OUT, NULL, NULL, NULL);

ilclient_change_component_state(video_encode, OMX_StateExecuting);


std::cout << "RPi JPEG encoder Ok" << std::endl;
#endif
return true;
}
*/

