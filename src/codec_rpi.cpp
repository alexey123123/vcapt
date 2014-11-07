#include <vector>
#include <algorithm>

#include "codec_rpi.h"


codec_rpi::codec_rpi():codec_libav(){
}
codec_rpi::~codec_rpi(){
}


packet_ptr codec_rpi::do_process_frame(frame_ptr fptr){

	packet_ptr ret_packet(new packet());
	ret_packet->frame_tp = fptr->tp;

	using namespace boost::chrono;


// 	fptr->avframe->pts = boost::chrono::duration_cast<boost::chrono::milliseconds>(fptr->tp - codec_start_tp).count();
// 	ret_packet->p.pts = fptr->avframe->pts;
//	ret_packet->p.dts = fptr->avframe->pts;

	AVFrame fr;
	memcpy(&fr,fptr->avframe,sizeof(AVFrame));
	fr.pts = boost::chrono::duration_cast<boost::chrono::milliseconds>(fptr->tp - codec_start_tp).count();

	if (!omx_helper.encode_frame(&fr,ret_packet->p))
		ret_packet = packet_ptr();

	/*
	if (ret_packet){
		ret_packet->p.pts = boost::chrono::duration_cast<boost::chrono::milliseconds>(fptr->tp - codec_start_tp).count();
		ret_packet->p.dts = ret_packet->p.pts;

	}
	*/


	return ret_packet;
}

void codec_rpi::do_initilalize(const format& f, AVPixelFormat& _codec_pixfmt){

	if ((f.codec_id != AV_CODEC_ID_H264))
		throw std::runtime_error("RPI supports only h264 formats");


	codec_libav::do_initilalize(f,_codec_pixfmt);//	need a fake libav codec initialization, because we need AVCodecContext
												 // for streaming h264 throw libav containers...		


	//now init hardware
	if (f.codec_id==AV_CODEC_ID_H264){
		omx_helper.init_h264_encoder(f.fsize.width,f.fsize.height,f.bitrate);
	}
		
		
//	TODO:
// 	if (f.name=="mjpeg")
// 		if (!pimpl->initialize_jpeg(f.fsize.width,f.fsize.height))
// 			throw std::runtime_error("RPI hardware error");

}

void codec_rpi::do_get_header_packets(std::deque<packet_ptr>& packets){
	omx_helper.get_header_packets(packets);
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

