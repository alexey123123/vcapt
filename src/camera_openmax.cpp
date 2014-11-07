#include <deque>

#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/chrono/chrono.hpp>

#include "camera_openmax.h"
#include "types.h"


#if defined(OPENMAX_IL)
#pragma pack(4)
#include <interface/vcos/vcos_assert.h>
#include <interface/vcos/vcos_semaphore.h>
#include <interface/vmcs_host/vchost.h>

#include <IL/OMX_Index.h>
#include <IL/OMX_Core.h>
#include <IL/OMX_Component.h>
#include <IL/OMX_Video.h>
#include <IL/OMX_Broadcom.h>
#pragma pack()

#define CAM_DEVICE_NUMBER     0


#define OMX_INIT_STRUCTURE(a) \
	memset(&(a), 0, sizeof(a)); \
	(a).nSize = sizeof(a); \
	(a).nVersion.nVersion = OMX_VERSION; \
	(a).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
	(a).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
	(a).nVersion.s.nRevision = OMX_VERSION_REVISION; \
	(a).nVersion.s.nStep = OMX_VERSION_STEP

OMX_ERRORTYPE event_handler_ext(
	OMX_HANDLETYPE hComponent,
	OMX_PTR pAppData,
	OMX_EVENTTYPE eEvent,
	OMX_U32 nData1,
	OMX_U32 nData2,
	OMX_PTR pEventData);

OMX_ERRORTYPE fill_output_buffer_done_handler_ext(
	OMX_HANDLETYPE hComponent,
	OMX_PTR pAppData,
	OMX_BUFFERHEADERTYPE* pBuffer);


void throw_std_runtime_error(const char* format, ...){

	va_list fmtargs;
	va_start(fmtargs, format);
	char fmtstr[4096];
	vsprintf(fmtstr, format, fmtargs);
	va_end(fmtargs);	

	throw std::runtime_error(fmtstr);
}


class omx_semaphore_locker{
public:
	omx_semaphore_locker(VCOS_SEMAPHORE_T h):sem_handler(h){
		vcos_semaphore_wait(&sem_handler);
	};
	~omx_semaphore_locker(){
		vcos_semaphore_post(&sem_handler);
	};
private:
	VCOS_SEMAPHORE_T sem_handler;
};

#endif

#define OMX_BUFFERS_MAX_COUNT 6
#define TIMEOUT_TO_BUFFERS_RETURN_MS 5000
#define OMX_CAMERA_CMP_OUT_PORT 70


class camera_openmax::Impl{
public:
	Impl(camera::state_change_handler _state_h, camera::stop_handler _stop_h):
	  current_buff_index(0),
	  last_returned_buffer_index(0),
	  actual_buffers_count(0){
#if defined(OPENMAX_IL)
		flushed = 0;
		camera_ready = 0;
		current_omx_camera_state = OMX_StateMax;
#else
		throw std::runtime_error("openmax not defined");
#endif
		omx_camera_started.exchange(false,boost::memory_order_release);


		need_stop_thread.exchange(false,boost::memory_order_release);
		//internal_thread = boost::thread(boost::bind(&Impl::do_thread_work,this));

	};
	~Impl(){
#if defined(OPENMAX_IL)

#endif
		need_stop_thread.exchange(true,boost::memory_order_release);
		//internal_thread.join();
	}

#if defined(OPENMAX_IL)
	VCOS_SEMAPHORE_T handler_lock;
	OMX_HANDLETYPE camera_handle;
	boost::atomic<int> flushed;
	int camera_ready;




	void init_component_handle(
		const char *name,
		OMX_HANDLETYPE* hComponent,
		OMX_PTR pAppData,
		OMX_CALLBACKTYPE* callbacks) {
			OMX_ERRORTYPE r;
			char fullname[32];

			// Get handle
			memset(fullname, 0, sizeof(fullname));
			strcat(fullname, "OMX.broadcom.");
			strncat(fullname, name, strlen(fullname) - 1);
			printf("Initializing component %s", fullname);
			if((r = OMX_GetHandle(hComponent, fullname, pAppData, callbacks)) != OMX_ErrorNone)
				throw_std_runtime_error("Failed to get handle for component (%08X)",r);

			// Disable ports
			OMX_INDEXTYPE types[] = {
				OMX_IndexParamAudioInit,
				OMX_IndexParamVideoInit,
				OMX_IndexParamImageInit,
				OMX_IndexParamOtherInit
			};
			OMX_PORT_PARAM_TYPE ports;
			OMX_INIT_STRUCTURE(ports);
			OMX_GetParameter(*hComponent, OMX_IndexParamVideoInit, &ports);

			int i;
			for(i = 0; i < 4; i++) {
				if(OMX_GetParameter(*hComponent, types[i], &ports) == OMX_ErrorNone) {
					OMX_U32 nPortIndex;
					for(nPortIndex = ports.nStartPortNumber; nPortIndex < ports.nStartPortNumber + ports.nPorts; nPortIndex++) {
						printf("Disabling port %d of component %s", nPortIndex, fullname);
						if((r = OMX_SendCommand(*hComponent, OMX_CommandPortDisable, nPortIndex, NULL)) != OMX_ErrorNone) 
							throw_std_runtime_error("failed to disable port %d: error %08X",nPortIndex,r);
						block_until_port_changed(*hComponent, nPortIndex, OMX_FALSE);
					}
				}
			}
	}

	void block_until_port_changed(OMX_HANDLETYPE hComponent, OMX_U32 nPortIndex, OMX_BOOL bEnabled) {
		OMX_ERRORTYPE r;
		OMX_PARAM_PORTDEFINITIONTYPE portdef;
		OMX_INIT_STRUCTURE(portdef);
		portdef.nPortIndex = nPortIndex;
		OMX_U32 i = 0;
		while(i++ == 0 || portdef.bEnabled != bEnabled) {
			if((r = OMX_GetParameter(hComponent, OMX_IndexParamPortDefinition, &portdef)) != OMX_ErrorNone)
				throw_std_runtime_error("failed to get port(%d) definition. error:%08X",nPortIndex,r);
			if(portdef.bEnabled != bEnabled) {
				usleep(10000);
			}
		}
	}
	void block_until_state_changed(OMX_HANDLETYPE hComponent, OMX_STATETYPE wanted_eState) {
		OMX_STATETYPE eState;
		int i = 0;
		while(i++ == 0 || eState != wanted_eState) {
			OMX_GetState(hComponent, &eState);
			if(eState != wanted_eState) {
				usleep(10000);
			}
		}
	}
	void block_untill_flushed(){
		while (flushed==0)
			usleep(10000);
	}

OMX_ERRORTYPE event_handler(
        OMX_HANDLETYPE hComponent,
        OMX_PTR pAppData,
        OMX_EVENTTYPE eEvent,
        OMX_U32 nData1,
        OMX_U32 nData2,
        OMX_PTR pEventData) {

	   switch(eEvent) {
        case OMX_EventCmdComplete:
            vcos_semaphore_wait(&handler_lock);
            if(nData1 == OMX_CommandFlush) {
                flushed.exchange(1,boost::memory_order_release);
            }
            vcos_semaphore_post(&handler_lock);
            break;
        case OMX_EventParamOrConfigChanged:
            vcos_semaphore_wait(&handler_lock);
            if(nData2 == OMX_IndexParamCameraDeviceNumber) {
                camera_ready = 1;
            }
            vcos_semaphore_post(&handler_lock);
            break;
        case OMX_EventError:
			if (nData1==OMX_ErrorSameState)
				break;
			if (nData1==OMX_ErrorPortUnpopulated)
				break;
			
			throw_std_runtime_error("error event received. nData1:%08X,nData2:%08X",nData1,nData2);
            break;
        default:
            break;
    }

    return OMX_ErrorNone;
}

// Called by OMX when the encoder component has filled
// the output buffer with H.264 encoded video data
OMX_ERRORTYPE fill_output_buffer_done_handler(
        OMX_HANDLETYPE hComponent,
        OMX_PTR pAppData,
        OMX_BUFFERHEADERTYPE* pBuffer) {

    if (hComponent == camera_handle){




		omx_buffer* b = (omx_buffer*)pBuffer->pAppPrivate;
		omx_buffer::e_state s = omx_buffer::s_error;
		if ((pBuffer->nFlags & OMX_BUFFERFLAG_ENDOFFRAME)>0)
			s = omx_buffer::s_filled;
		b->buff_index = current_buff_index;
		b->tp = boost::chrono::steady_clock::now();
		current_buff_index++;

		//mark buffer
		b->state.exchange(s,boost::memory_order_release);


		/*
		printf("fill_output_buffer_done_handler (%d bytes, flags:%08X, state:%d)\n",pBuffer->nFilledLen,
			pBuffer->nFlags,
			pBuffer->nTimeStamp,
			(int)s);
		*/
	}
    return OMX_ErrorNone;
}
#endif//#if defined(OPENMAX_IL)


	void do_connect(const camera::connect_parameters& params){
#if defined(OPENMAX_IL)
		//try to init rpi camera
		if(vcos_semaphore_create(&handler_lock, "handler_lock", 1) != VCOS_SUCCESS) {
			throw std::runtime_error("Failed to create handler lock semaphore");
		}

		// Init component handles
		OMX_CALLBACKTYPE callbacks;
		memset(&callbacks, 0, sizeof(callbacks));
		callbacks.EventHandler   = event_handler_ext;
		callbacks.FillBufferDone = fill_output_buffer_done_handler_ext;

		init_component_handle("camera", &camera_handle , this, &callbacks);

		int r;

		// Request a callback to be made when OMX_IndexParamCameraDeviceNumber is
		// changed signaling that the camera device is ready for use.
		OMX_CONFIG_REQUESTCALLBACKTYPE cbtype;
		OMX_INIT_STRUCTURE(cbtype);
		cbtype.nPortIndex = OMX_ALL;
		cbtype.nIndex     = OMX_IndexParamCameraDeviceNumber;
		cbtype.bEnable    = OMX_TRUE;
		if((r = OMX_SetConfig(camera_handle, OMX_IndexConfigRequestCallback, &cbtype)) != OMX_ErrorNone) {
			throw_std_runtime_error("Failed to request camera device number parameter change callback for camera. error %08X",r);
		}
		// Set device number, this triggers the callback configured just above
		OMX_PARAM_U32TYPE device;
		OMX_INIT_STRUCTURE(device);
		device.nPortIndex = OMX_ALL;
		device.nU32 = CAM_DEVICE_NUMBER;
		if((r = OMX_SetParameter(camera_handle, OMX_IndexParamCameraDeviceNumber, &device)) != OMX_ErrorNone) {
			throw_std_runtime_error("Failed to set camera parameter device number. error %08X",r);
		}

		OMX_PARAM_PORTDEFINITIONTYPE camera_portdef;
		OMX_INIT_STRUCTURE(camera_portdef);
		camera_portdef.nPortIndex = OMX_CAMERA_CMP_OUT_PORT;
		if((r = OMX_GetParameter(camera_handle, OMX_IndexParamPortDefinition, &camera_portdef)) != OMX_ErrorNone) {
			throw_std_runtime_error("Failed to get port definition for camera preview output port. error %08X",r);
		}

		printf("Switching state of the camera component to idle...\n");
		if (!check_and_change_component_state(camera_handle,OMX_StateIdle))
			throw_std_runtime_error("Failed to switch state of the camera component to idle");



		current_framesize = frame_size(camera_portdef.format.video.nFrameWidth,
			camera_portdef.format.video.nFrameHeight);

		printf("camera component created\n");

#endif
	}

	void do_disconnect(){
#if defined(OPENMAX_IL)
		// Free the component handles
		int r;
		if ((r = OMX_FreeHandle(camera_handle)) != OMX_ErrorNone) {
			throw_std_runtime_error("Failed to free camera component handle. error %08X",r);
		}
#endif//#if defined(OPENMAX_IL)
	}

	capturer::definition do_get_definition() const{
		capturer::definition def;

		def.bus_info = "CSI";
		def.device_name = "OpenMax camera";
		def.manufacturer_name = "unknown";
		def.slot_name = "1";

		return def;
	}
	capturer::capabilities do_get_capabilities() const{
		capturer::capabilities caps;
		caps.flags = capturer::capabilities::f_start_stop_streaming;
		return caps;
	}

	capturer::format do_get_current_format() const{
		capturer::format f;
		f.ffmpeg_pixfmt = AV_PIX_FMT_YUV420P;
		f.type = capturer::format::fst_discrete;
		f.framesizes.push_back(frame_size(320,240));
		f.framesizes.push_back(frame_size(640,480));
		f.framesizes.push_back(frame_size(1280,720));
		f.framesizes.push_back(frame_size(1920,1080));
		return f;
	}

	frame_size current_framesize;

	frame_size do_get_framesize(){
		return current_framesize;
	}


	boost::atomic<bool> omx_camera_started;
	struct omx_buffer{
#if defined(OPENMAX_IL)
		OMX_BUFFERHEADERTYPE *buf;		
#endif
		boost::int64_t buff_index;
		boost::chrono::steady_clock::time_point tp;
		enum e_state{
			s_unallocated,
			s_empty,	//
			s_filling,	//transfered to openmax for fill
			s_filled,	//filled by openmax
			s_error,	//some error, returned by openmax
			s_owned		//owned by get_frame()
		};
		boost::atomic<e_state> state;
		
		omx_buffer(){
#if defined(OPENMAX_IL)
			buf = 0;		
#endif
			state.exchange(s_unallocated,boost::memory_order_release);
		};

	};
	typedef boost::shared_ptr<omx_buffer> omx_buffer_ptr;
	boost::int64_t current_buff_index;
	omx_buffer buf_73;
	//std::deque<omx_buffer_ptr> allocated_buffers;
	omx_buffer omx_buffers[OMX_BUFFERS_MAX_COUNT];
	int actual_buffers_count;
	boost::thread internal_thread;
	boost::atomic<bool> need_stop_thread;
	




	void do_stop_streaming(){
		
#if defined(OPENMAX_IL)
		int r;

		omx_semaphore_locker l1(handler_lock);

		omx_camera_started.exchange(false,boost::memory_order_release);


		//wait for return all buffers in 5 sec
		using namespace boost::chrono;
		steady_clock::time_point stp = steady_clock::now();
		steady_clock::time_point np;
		uint64_t elapsed_ms = 0;
		while (elapsed_ms < TIMEOUT_TO_BUFFERS_RETURN_MS){
			bool all_returned = true; 
			for (int i=0;i!=actual_buffers_count;i++)
				if (omx_buffers[i].buf)
					all_returned &= omx_buffers[i].state != omx_buffer::s_filling;
			if (all_returned)
				break;
			np = steady_clock::now();
			elapsed_ms = duration_cast<milliseconds> (np - stp).count();
			boost::this_thread::sleep(boost::posix_time::milliseconds(100));
		}
		np = steady_clock::now();
		elapsed_ms = duration_cast<milliseconds> (np - stp).count();
		printf("all buffers returned (%d ms)\n");

		// Return the last full buffer back to the encoder component
		omx_buffer* fbuffer = &omx_buffers[0];
		if (fbuffer->buf){
			fbuffer->buf->nFlags = OMX_BUFFERFLAG_EOS;
			if((r = OMX_FillThisBuffer(camera_handle, fbuffer->buf)) != OMX_ErrorNone) {
				printf("Failed to request filling of the output buffer on camera output port (%08X)",r);
			}
			printf("fbuffer returned\n");

		}

		// Flush the buffers on camera
		flushed.exchange(0,boost::memory_order_release);
		if((r = OMX_SendCommand(camera_handle, OMX_CommandFlush, 73, NULL)) != OMX_ErrorNone) {
			printf("Failed to flush buffers of camera output port 73\n");
		}
		block_untill_flushed();
		//block_untill_fr
		// Flush the buffers on camera
		flushed.exchange(0,boost::memory_order_release);
		if((r = OMX_SendCommand(camera_handle, OMX_CommandFlush, OMX_CAMERA_CMP_OUT_PORT, NULL)) != OMX_ErrorNone) {
			printf("Failed to flush buffers of camera output port\n");
		}
		block_untill_flushed();

		//free buffers
		if (buf_73.buf)
			if((r = OMX_FreeBuffer(camera_handle, 73, buf_73.buf)) != OMX_ErrorNone) {
				printf("Failed to free buffer for camera input port 73\n");
			}
		buf_73.buf = 0;

		if (actual_buffers_count)
			for (int i=0;i!=actual_buffers_count;i++){
				if (omx_buffers[i].buf)
					if((r = OMX_FreeBuffer(camera_handle, OMX_CAMERA_CMP_OUT_PORT, omx_buffers[i].buf)) != OMX_ErrorNone) {
						printf("Failed to free buffer for camera output port\n");
					}
				omx_buffers[i].buf = 0;
				omx_buffers[i].state.exchange(omx_buffer::s_unallocated,boost::memory_order_release);
			}
		actual_buffers_count = 0;

		// Disable all the ports
		if((r = OMX_SendCommand(camera_handle, OMX_CommandPortDisable, 73, NULL)) != OMX_ErrorNone) {
			printf("Failed to disable camera input port 73 (%08X)\n",r);
		}
		block_until_port_changed(camera_handle, 73, OMX_FALSE);
		if((r = OMX_SendCommand(camera_handle, OMX_CommandPortDisable, OMX_CAMERA_CMP_OUT_PORT, NULL)) != OMX_ErrorNone) {
			printf("Failed to disable camera output port(%08X)\n",r);
		}
		block_until_port_changed(camera_handle, OMX_CAMERA_CMP_OUT_PORT, OMX_FALSE);


		// Switch components to idle state
		printf("Switching state of the camera component to idle...\n");
		if (!check_and_change_component_state(camera_handle,OMX_StateIdle))
			printf("Failed to switch state of the camera component to idle\n");

		printf("omx camera stopped\n");
#endif//
		

	}

#if defined(OPENMAX_IL)
	OMX_STATETYPE current_omx_camera_state;
	bool check_and_change_component_state(OMX_HANDLETYPE h,OMX_STATETYPE s){
		if (current_omx_camera_state==s)
			return true;
		int r;
		if((r = OMX_SendCommand(h, OMX_CommandStateSet, s, NULL)) != OMX_ErrorNone) {
			printf("OMX_CommandStateSet error:%08X\n",r);
			return false;
		}
			
		block_until_state_changed(camera_handle, s);
		current_omx_camera_state = s;
		return true;
	}
#endif//


	void do_start_streaming(){

		if (omx_camera_started)
			throw std::runtime_error("streaming is already started");
#if defined(OPENMAX_IL)
		int r;

		omx_semaphore_locker l1(handler_lock);

		printf("Switching state of the camera component to idle...\n");
		if (!check_and_change_component_state(camera_handle, OMX_StateIdle))
			throw std::runtime_error("Failed to switch state of the camera component to idle");


		OMX_PARAM_PORTDEFINITIONTYPE camera_portdef;
		OMX_INIT_STRUCTURE(camera_portdef);
		camera_portdef.nPortIndex = OMX_CAMERA_CMP_OUT_PORT;
		if((r = OMX_GetParameter(camera_handle, OMX_IndexParamPortDefinition, &camera_portdef)) != OMX_ErrorNone) {
			throw_std_runtime_error("Failed to get port definition for camera preview output port 70. error %08X",r);

		}
		printf("(1)camera_portdef.nBufferSize=%d,w:h=%dx%d,nBufferCountMin:%d\n",
			camera_portdef.nBufferSize,
			camera_portdef.format.video.nFrameWidth,
			camera_portdef.format.video.nFrameHeight,
			camera_portdef.nBufferCountMin);

		camera_portdef.format.video.nFrameWidth  = current_framesize.width;
		camera_portdef.format.video.nFrameHeight = current_framesize.height;
		camera_portdef.format.video.xFramerate   = 30 << 16;
		// Stolen from gstomxvideodec.c of gst-omx
		camera_portdef.format.video.nSliceHeight = camera_portdef.format.video.nFrameHeight;
		camera_portdef.format.video.nStride      = (camera_portdef.format.video.nFrameWidth + camera_portdef.nBufferAlignment - 1) & (~(camera_portdef.nBufferAlignment - 1));
		camera_portdef.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
		camera_portdef.nBufferCountActual = OMX_BUFFERS_MAX_COUNT;
		//camera_portdef.bBuffersContiguous = OMX_FALSE;
		if((r = OMX_SetParameter(camera_handle, OMX_IndexParamPortDefinition, &camera_portdef)) != OMX_ErrorNone) {
			throw_std_runtime_error("Failed to set port definition for camera preview output port. error %08X",r);

		}	
		printf("(2)camera_portdef.nBufferSize=%d,w:h=%dx%d,nBufferCountMin:%d\n",
			camera_portdef.nBufferSize,
			camera_portdef.format.video.nFrameWidth,
			camera_portdef.format.video.nFrameHeight,
			camera_portdef.nBufferCountMin);
		actual_buffers_count = camera_portdef.nBufferCountActual;


		// Configure frame rate
		OMX_CONFIG_FRAMERATETYPE framerate;
		OMX_INIT_STRUCTURE(framerate);
		framerate.nPortIndex = OMX_CAMERA_CMP_OUT_PORT;
		framerate.xEncodeFramerate = camera_portdef.format.video.xFramerate;
		if((r = OMX_SetConfig(camera_handle, OMX_IndexConfigVideoFramerate, &framerate)) != OMX_ErrorNone) {
			throw std::runtime_error("Failed to set framerate configuration for camera output port");
		}

		//Timestamps
		
		/*
		OMX_PARAM_TIMESTAMPMODETYPE tsm;
		OMX_INIT_STRUCTURE(tsm);
		//tsm.eTimestampMode = OMX_TimestampModeRawStc;
		tsm.eTimestampMode = OMX_TimestampModeZero;
		if((r = OMX_SetConfig(camera_handle, OMX_IndexParamCommonUseStcTimestamps, &tsm)) != OMX_ErrorNone) {
			printf("Failed to set TimestampMode:%08X",r);
			throw std::runtime_error("Failed to set TimestampMode");
		}
		*/



		// Ensure camera is ready
		while(!camera_ready) {
			usleep(10000);
		}

		// Enable ports
		printf("Enabling ports...\n");
		if((r = OMX_SendCommand(camera_handle, OMX_CommandPortEnable, 73, NULL)) != OMX_ErrorNone) {
			throw std::runtime_error("Failed to enable camera input port 73");
		}
		block_until_port_changed(camera_handle, 73, OMX_TRUE);

		if((r = OMX_SendCommand(camera_handle, OMX_CommandPortEnable, OMX_CAMERA_CMP_OUT_PORT, NULL)) != OMX_ErrorNone) {
			throw std::runtime_error("Failed to enable camera preview output port");
		}
		block_until_port_changed(camera_handle, OMX_CAMERA_CMP_OUT_PORT, OMX_TRUE);

		printf("Allocating buffers...\n");

		OMX_INIT_STRUCTURE(camera_portdef);
		camera_portdef.nPortIndex = 73;
		if((r = OMX_GetParameter(camera_handle, OMX_IndexParamPortDefinition, &camera_portdef)) != OMX_ErrorNone) {
			throw_std_runtime_error("Failed to get port definition for camera input port 73. error %08X",r);
		}

		if((r = OMX_AllocateBuffer(camera_handle, &buf_73.buf, 73, NULL, camera_portdef.nBufferSize)) != OMX_ErrorNone) {
			throw_std_runtime_error("Failed to allocate buffer for camera input port 73. error %08X",r);
		}
		printf("allocated buffer for port 73 (size:%d)\n",camera_portdef.nBufferSize);


		OMX_INIT_STRUCTURE(camera_portdef);
		camera_portdef.nPortIndex = OMX_CAMERA_CMP_OUT_PORT;
		if((r = OMX_GetParameter(camera_handle, OMX_IndexParamPortDefinition, &camera_portdef)) != OMX_ErrorNone) {
			throw_std_runtime_error("Failed to get port definition for camera preview output port 70. error %08X",r);
		}

		if (actual_buffers_count)
		for (int i=0;i!=actual_buffers_count;i++){
			if((r = OMX_AllocateBuffer(camera_handle, &(omx_buffers[i].buf), OMX_CAMERA_CMP_OUT_PORT, 
						NULL, camera_portdef.nBufferSize)) != OMX_ErrorNone) {
				throw_std_runtime_error("Failed to allocate buffer for camera output port. error %08X",r);
			}
			printf("buffer allocated (%d/%d bytes), flags:%08X\n",omx_buffers[i].buf->nAllocLen,camera_portdef.nBufferSize,omx_buffers[i].buf->nFlags);
			omx_buffers[i].buf->pAppPrivate = (void*)&omx_buffers[i];
		}

		printf("Switching state of the camera component to executing...\n");
		if (!check_and_change_component_state(camera_handle,OMX_StateExecuting))
			throw std::runtime_error("Failed to switch state of the camera component to executing");


		/*
		printf("Switching on capture on camera video output port ...\n");
		OMX_CONFIG_PORTBOOLEANTYPE capture;
		OMX_INIT_STRUCTURE(capture);
		capture.nPortIndex = OMX_CAMERA_CMP_OUT_PORT;
		capture.bEnabled = OMX_TRUE;
		if((r = OMX_SetParameter(camera_handle, OMX_IndexConfigPortCapturing, &capture)) != OMX_ErrorNone) {
			throw_std_runtime_error("Failed to switch on capture on camera video output port. error %08X",r);
		}
		*/

		last_returned_buffer_index = -1;
		current_buff_index = 0;

		if (actual_buffers_count)
		for (int i=0;i != actual_buffers_count;i++){
			if((r = OMX_FillThisBuffer(camera_handle, omx_buffers[i].buf)) != OMX_ErrorNone) {
				throw_std_runtime_error("Failed to request filling of the output buffer on camera output port. error %08X",r);
			}
		}
		printf("omx camera started\n");
#endif
		omx_camera_started.exchange(true,boost::memory_order_release);
	}


	spinlock frames_spinlock;

	/*
	void do_thread_work(){
		while (!need_stop_thread){

			if (!omx_camera_started){
				boost::this_thread::sleep(boost::posix_time::milliseconds(200));
				continue;
			}

			//TODO: spinlock
			frames_spinlock.lock();

			//try to find last filled (and not owned) buffer
			omx_buffer* next_to_fill = 0;
			boost::int64_t max_buff_index = 0x7FFFFFFFFFFFFFFF;

			for (int i=0;i!=OMX_BUFFERS_MAX_COUNT;i++){
				omx_buffer* ob = &omx_buffers[i];
				if (ob->state == omx_buffer::s_owned)
					continue;
				if (ob->state == omx_buffer::s_unallocated)
					continue;
				if (ob->state == omx_buffer::s_filling)
					continue;


				if (ob->buff_index < max_buff_index){
					next_to_fill = ob;
					max_buff_index = ob->buff_index;
				}
			}

			//mark as "filling"
			if (next_to_fill)
				next_to_fill->state.exchange(omx_buffer::s_filling,boost::memory_order_release);
			

			frames_spinlock.unlock();//unlocking and then OMX_FillThisBuffer

			if (next_to_fill){
#if defined(OPENMAX_IL)
				if((OMX_FillThisBuffer(camera_handle, next_to_fill->buf)) != OMX_ErrorNone) {
					printf("Failed to request filling of the output buffer on camera output port 71\n");
					next_to_fill->state.exchange(omx_buffer::s_error,boost::memory_order_release);
					next_to_fill->buff_index = current_buff_index;
				}
#endif//
			}


			usleep(35000);

		}
	}
	*/


	boost::chrono::steady_clock::time_point last_client_tp;
	boost::int64_t last_returned_buffer_index;


	frame_ptr do_get_frame(boost::chrono::steady_clock::time_point last_frame_tp){
		if (!omx_camera_started)
			throw std::runtime_error("streaming is not started");

		boost::int64_t min_index = -1;
		if (last_frame_tp > last_client_tp)
			min_index = last_returned_buffer_index;

		//get ready frame
		frame_ptr fptr;
		
		omx_buffer* next_to_fill = 0;
		omx_buffer* ret_b = 0;
		
		frames_spinlock.lock();
		if (actual_buffers_count)
		for (int i=0;i!=actual_buffers_count;i++){
			omx_buffer* ob = &omx_buffers[i];

			//looking next to fill
			if ((ob->state == omx_buffer::s_empty)||(ob->state == omx_buffer::s_error))
				next_to_fill = ob;

			if (ob->state != omx_buffer::s_filled)
				continue;
			if (ob->buff_index <= min_index)
				continue ;


			//mark as extracted
			ob->state.exchange(omx_buffer::s_owned,boost::memory_order_release);
			//printf("owned %08X\n",ob);
			ret_b = ob;
			break;
		}
		frames_spinlock.unlock();

		if (ret_b){
			fptr = frame_ptr(new frame());
			//fptr->tp = boost::chrono::steady_clock::now();
			fptr->tp = ret_b->tp;
			fptr->dcb = boost::bind(&camera_openmax::Impl::do_return_frame,this,fptr->tp,ret_b);

			//construct avframe
			fptr->avframe = libav::av_frame_alloc();
#if defined(OPENMAX_IL)
			int _size = libav::avpicture_fill((AVPicture *)fptr->avframe, 
				(uint8_t*)(ret_b->buf->pBuffer + ret_b->buf->nOffset), AV_PIX_FMT_YUV420P, 
				current_framesize.width, current_framesize.height);
#endif
			fptr->avframe->width = current_framesize.width;
			fptr->avframe->height = current_framesize.height;			
			fptr->avframe->format = int(AV_PIX_FMT_YUV420P);



			last_client_tp = fptr->tp;
			last_returned_buffer_index = ret_b->buff_index;
		}

		
		if (next_to_fill){
			//mark as "filling"
			//printf("filling %08X",next_to_fill);
			next_to_fill->state.exchange(omx_buffer::s_filling,boost::memory_order_release);
#if defined(OPENMAX_IL)
			if((OMX_FillThisBuffer(camera_handle, next_to_fill->buf)) != OMX_ErrorNone) {
				printf("Failed to request filling of the output buffer on camera output port\n");
				next_to_fill->state.exchange(omx_buffer::s_error,boost::memory_order_release);
				next_to_fill->buff_index = current_buff_index;
			}
#endif//

		}
		

		return fptr;
	}

	void do_return_frame(boost::chrono::steady_clock::time_point tp, void* opaque){
		omx_buffer* ob = (omx_buffer*)opaque;
		//mark as returned
		ob->state.exchange(omx_buffer::s_empty,boost::memory_order_release);
		//printf("returned %08X\n",ob);

		if (!omx_camera_started)
			return ;


		ob->state.exchange(omx_buffer::s_filling,boost::memory_order_release);
#if defined(OPENMAX_IL)
		if((OMX_FillThisBuffer(camera_handle, ob->buf)) != OMX_ErrorNone) {
			printf("Failed to request filling of the output buffer on camera output port\n");
			ob->state.exchange(omx_buffer::s_error,boost::memory_order_release);
			ob->buff_index = current_buff_index;
		}
#endif//

		//find next to fill

	}

	void do_set_framesize(const frame_size& fsize){
		if (omx_camera_started)
			throw std::runtime_error("stream not stopped");

		//check fsize
		camera::format f = do_get_current_format();
		if (!f.check_framesize(fsize))
			throw std::runtime_error("framesize not stopped");
		current_framesize = fsize;
	}


};


#if defined(OPENMAX_IL)
static void dump_event(OMX_HANDLETYPE hComponent, OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2) {
	char e[255];
	memset(e,0,255);
	switch(eEvent) {
	case OMX_EventCmdComplete:          strcat(e,"command complete");                   break;
	case OMX_EventError:                strcat(e,"error");                              break;
	case OMX_EventParamOrConfigChanged: strcat(e,"parameter or configuration changed"); break;
	case OMX_EventPortSettingsChanged:  strcat(e,"port settings changed");              break;
		/* That's all I've encountered during hacking so let's not bother with the rest... */
	default:
		strcat(e,"(no description)");
	}
	printf("Received event 0x%08x %s, hComponent:0x%08x, nData1:0x%08x, nData2:0x%08x\n",
		eEvent, e, hComponent, nData1, nData2);
}


OMX_ERRORTYPE event_handler_ext(
        OMX_HANDLETYPE hComponent,
        OMX_PTR pAppData,
        OMX_EVENTTYPE eEvent,
        OMX_U32 nData1,
        OMX_U32 nData2,
        OMX_PTR pEventData) {

    dump_event(hComponent, eEvent, nData1, nData2);
	camera_openmax::Impl *i = ((camera_openmax::Impl *)pAppData);
	return i->event_handler(hComponent,pAppData,eEvent,nData1,nData2,pEventData);

}

// Called by OMX when the encoder component has filled
// the output buffer with H.264 encoded video data
OMX_ERRORTYPE fill_output_buffer_done_handler_ext(
        OMX_HANDLETYPE hComponent,
        OMX_PTR pAppData,
        OMX_BUFFERHEADERTYPE* pBuffer) {
    camera_openmax::Impl *i = ((camera_openmax::Impl *)pAppData);
	return i->fill_output_buffer_done_handler(hComponent,pAppData,pBuffer);
}
#endif


camera_openmax::camera_openmax(const capturer::connect_parameters& _cp,state_change_handler _state_h, stop_handler _stop_h):
camera(_cp,_state_h,_stop_h){
	pimpl = new Impl(_state_h,_stop_h);
}
camera_openmax::~camera_openmax(){
	if (pimpl)
		delete pimpl;
}

void camera_openmax::do_connect(const connect_parameters& params){
	pimpl->do_connect(params);
}
void camera_openmax::do_disconnect(){

}

frame_ptr camera_openmax::do_get_frame(boost::chrono::steady_clock::time_point last_frame_tp){
	return pimpl->do_get_frame(last_frame_tp);
}
void camera_openmax::do_return_frame(boost::chrono::steady_clock::time_point tp, void* opaque){
	pimpl->do_return_frame(tp,opaque);
}

camera::format camera_openmax::do_get_current_format() const{
	return pimpl->do_get_current_format();
}
void camera_openmax::do_set_framesize(const frame_size& fsize){
	pimpl->do_set_framesize(fsize);
}
frame_size camera_openmax::do_get_framesize(){
	return pimpl->do_get_framesize();
}
camera::definition camera_openmax::do_get_definition() const{
	return pimpl->do_get_definition();
}
camera::capabilities camera_openmax::do_get_capabilities() const{
	return pimpl->do_get_capabilities();
}

void camera_openmax::do_start_streaming(){
	pimpl->do_start_streaming();
}
void camera_openmax::do_stop_streaming(){
	pimpl->do_stop_streaming();
}


