#include <boost/shared_ptr.hpp>

#include <utility/Library.h>
#include <system/Platform.h>


#include "libav.h"





typedef AVCodec* (*f_avcodec_find_encoder)(enum AVCodecID id);
typedef AVCodec* (*f_avcodec_find_decoder)(enum AVCodecID id);
typedef AVFrame* (*f_avcodec_alloc_frame)(void);
typedef int		(*f_avcodec_open2)(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
typedef int		(*f_avcodec_close)(AVCodecContext *avctx);
typedef AVCodecContext* (*f_avcodec_alloc_context3)(const AVCodec *codec);
typedef void	(*f_avcodec_register_all)(void);
typedef int		(*f_avcodec_encode_video2)(AVCodecContext *avctx, AVPacket *avpkt,const AVFrame *frame, int *got_packet_ptr);
typedef int		(*f_avcodec_decode_video2)(AVCodecContext *avctx, AVFrame *picture,int *got_picture_ptr, const AVPacket *avpkt);
typedef int		(*f_avpicture_alloc)(AVPicture *picture, enum AVPixelFormat pix_fmt, int width, int height);
typedef int		(*f_avpicture_fill)(AVPicture *picture, const uint8_t *ptr,enum AVPixelFormat pix_fmt, int width, int height);
typedef void	(*f_av_picture_copy)(AVPicture *dst, const AVPicture *src,	enum AVPixelFormat pix_fmt, int width, int height);
typedef int		(*f_avpicture_layout)(const AVPicture *src, enum AVPixelFormat pix_fmt,	int width, int height,	unsigned char *dest, int dest_size);
typedef void	(*f_avpicture_free)(AVPicture *picture);
typedef void	(*f_av_init_packet)(AVPacket* pkt);
typedef void	(*f_av_free_packet)(AVPacket *pkt);
typedef const char* (*f_avcodec_get_name)(enum AVCodecID id);


typedef struct SwsContext* (*f_sws_getContext)(int srcW, int srcH, enum AVPixelFormat srcFormat,
	int dstW, int dstH, enum AVPixelFormat dstFormat,
	int flags, SwsFilter *srcFilter,
	SwsFilter *dstFilter, const double *param);
typedef int (*f_sws_scale)(struct SwsContext *c, const uint8_t *const srcSlice[],
	const int srcStride[], int srcSliceY, int srcSliceH,
	uint8_t *const dst[], const int dstStride[]);
typedef void (*f_sws_freeContext)(struct SwsContext *swsContext);

typedef void (*f_av_free)(void *ptr);
typedef void (*f_av_freep)(void *ptr);
typedef AVFrame *(*f_av_frame_alloc)(void);
typedef void (*f_av_frame_free)(AVFrame **frame);
typedef void (*f_av_frame_unref)(AVFrame *frame);
typedef AVFrame *(*f_av_frame_clone)(const AVFrame *src);
typedef int (*f_av_frame_get_buffer)(AVFrame *frame, int align);
typedef int64_t (*f_av_rescale_q_rnd)(int64_t a, AVRational bq, AVRational cq, enum AVRounding);
typedef int64_t (*f_av_rescale_q)(int64_t a, AVRational bq, AVRational cq);
typedef int (*f_av_dict_set)(AVDictionary **pm, const char *key, const char *value, int flags);
typedef int (*f_av_dict_count)(const AVDictionary *m);
typedef const char* (*f_av_get_pix_fmt_name)(enum AVPixelFormat pix_fmt);
typedef int (*f_av_opt_set)(void *obj, const char *name, const char *val, int search_flags);
typedef char* (*f_av_strdup)(const char *s);
typedef unsigned int (*f_av_int_list_length_for_size)(unsigned int elsize,
	const void *list, uint64_t term);
typedef int (*f_av_opt_set_bin)(void *obj, const char *name, const uint8_t *val, int size, int search_flags);



typedef int (*f_avformat_alloc_output_context2)(AVFormatContext **ctx, AVOutputFormat *oformat,
	const char *format_name, const char *filename);
typedef void (*f_av_register_all)(void);
typedef AVStream* (*f_avformat_new_stream)(AVFormatContext *s, const AVCodec *c);
typedef void (*f_avformat_free_context)(AVFormatContext *s);

typedef AVIOContext *(*f_avio_alloc_context)(
	unsigned char *buffer,
	int buffer_size,
	int write_flag,
	void *opaque,
	int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
	int (*write_packet)(void *opaque, uint8_t *buf, int buf_size),
	int64_t (*seek)(void *opaque, int64_t offset, int whence));
typedef int (*f_avio_open)(AVIOContext **s, const char *url, int flags);
typedef int (*f_av_write_frame)(AVFormatContext *s, AVPacket *pkt);
typedef int (*f_av_interleaved_write_frame)(AVFormatContext *s, AVPacket *pkt);
typedef int (*f_avformat_write_header)(AVFormatContext *s, AVDictionary **options);
typedef int (*f_av_write_trailer)(AVFormatContext *s);


typedef void (*f_avfilter_register_all)(void);
typedef AVFilter* (*f_avfilter_get_by_name)(const char *name);
typedef AVFilterInOut* (*f_avfilter_inout_alloc)(void);
typedef void (*f_avfilter_inout_free)(AVFilterInOut **inout);
typedef AVFilterGraph* (*f_avfilter_graph_alloc)(void);
typedef int (*f_avfilter_graph_create_filter)(AVFilterContext **filt_ctx, const AVFilter *filt,
	const char *name, const char *args, void *opaque,
	AVFilterGraph *graph_ctx);
typedef int (*f_avfilter_graph_parse_ptr)(AVFilterGraph *graph, const char *filters,
	AVFilterInOut **inputs, AVFilterInOut **outputs,
	void *log_ctx);
typedef int (*f_avfilter_graph_config)(AVFilterGraph *graphctx, void *log_ctx);
typedef int (*f_av_buffersrc_add_frame_flags)(AVFilterContext *buffer_src,
	AVFrame *frame, int flags);
typedef int (*f_av_buffersink_get_frame_flags)(AVFilterContext *ctx, AVFrame *frame, int flags);
typedef int (*f_av_buffersink_get_frame)(AVFilterContext *ctx, AVFrame *frame);
typedef void (*f_avfilter_graph_free)(AVFilterGraph **graph);


static f_avcodec_find_encoder _avcodec_find_encoder = 0;
static f_avcodec_find_decoder _avcodec_find_decoder = 0;
static f_avcodec_alloc_frame _avcodec_alloc_frame = 0;
static f_avcodec_open2 _avcodec_open2 = 0;
static f_avcodec_close _avcodec_close = 0;
static f_avcodec_alloc_context3 _avcodec_alloc_context3 = 0;
static f_avcodec_register_all _avcodec_register_all = 0;
static f_avcodec_encode_video2 _avcodec_encode_video2 = 0;
static f_avcodec_decode_video2 _avcodec_decode_video2 = 0;
static f_avcodec_get_name _avcodec_get_name = 0;

static f_avpicture_fill _avpicture_fill = 0;
static f_av_picture_copy _av_picture_copy = 0;
static f_av_init_packet _av_init_packet = 0;
static f_av_free_packet _av_free_packet = 0;
static f_avpicture_alloc _avpicture_alloc = 0;
static f_avpicture_layout _avpicture_layout = 0;
static f_avpicture_free _avpicture_free = 0;

static f_sws_getContext _sws_getContext = 0;
static f_sws_scale _sws_scale = 0;
static f_sws_freeContext _sws_freeContext = 0;


static f_av_free _av_free = 0;
static f_av_freep _av_freep = 0;
static f_av_frame_alloc _av_frame_alloc = 0;
static f_av_frame_free _av_frame_free = 0;
static f_av_frame_unref _av_frame_unref = 0;
static f_av_frame_clone _av_frame_clone = 0;
static f_av_frame_get_buffer _av_frame_get_buffer = 0;
static f_av_rescale_q_rnd _av_rescale_q_rnd = 0;
static f_av_rescale_q _av_rescale_q = 0;
static f_av_dict_set _av_dict_set = 0;
static f_av_dict_count _av_dict_count = 0;
static f_av_get_pix_fmt_name _av_get_pix_fmt_name = 0;
static f_av_opt_set _av_opt_set = 0;
static f_av_strdup _av_strdup = 0;
static f_av_int_list_length_for_size _av_int_list_length_for_size = 0;
static f_av_opt_set_bin _av_opt_set_bin = 0;




static f_avformat_alloc_output_context2 _avformat_alloc_output_context2 = 0;
static f_av_register_all _av_register_all = 0;
static f_avformat_new_stream _avformat_new_stream = 0;
static f_avformat_free_context _avformat_free_context = 0;
static f_avio_alloc_context _avio_alloc_context = 0;
static f_avio_open _avio_open = 0;
static f_av_write_frame _av_write_frame = 0;
static f_av_interleaved_write_frame _av_interleaved_write_frame = 0;
static f_avformat_write_header _avformat_write_header = 0;
static f_av_write_trailer _av_write_trailer = 0;

static f_avfilter_register_all _avfilter_register_all = 0;
static f_avfilter_get_by_name _avfilter_get_by_name = 0; 
static f_avfilter_inout_alloc _avfilter_inout_alloc = 0;
static f_avfilter_inout_free _avfilter_inout_free = 0;
static f_avfilter_graph_alloc _avfilter_graph_alloc = 0;
static f_avfilter_graph_create_filter _avfilter_graph_create_filter = 0;
static f_avfilter_graph_parse_ptr _avfilter_graph_parse_ptr = 0;
static f_avfilter_graph_config _avfilter_graph_config = 0;
static f_av_buffersrc_add_frame_flags _av_buffersrc_add_frame_flags = 0;
static f_av_buffersink_get_frame_flags _av_buffersink_get_frame_flags = 0;
static f_av_buffersink_get_frame _av_buffersink_get_frame = 0;
static f_avfilter_graph_free _avfilter_graph_free = 0;


typedef boost::shared_ptr<Utility::Library> LibraryPtr;

LibraryPtr avcodec_lib_ptr;
LibraryPtr swscale_lib_ptr;
LibraryPtr avutil_lib_ptr;
LibraryPtr avformat_lib_ptr;
LibraryPtr avfilter_lib_ptr;




bool libav::initialize(std::string& error_message){
	try{
#if defined(Win32Platform)
		avcodec_lib_ptr = LibraryPtr(new Utility::Library("avcodec-55.dll"));
		swscale_lib_ptr = LibraryPtr(new Utility::Library("swscale-2.dll"));
		avutil_lib_ptr = LibraryPtr(new Utility::Library("avutil-52.dll"));
		avformat_lib_ptr = LibraryPtr(new Utility::Library("avformat-55.dll"));
		avfilter_lib_ptr = LibraryPtr(new Utility::Library("avfilter-4.dll"));
#elif defined(LinuxPlatform)
		avcodec_lib_ptr = LibraryPtr(new Utility::Library("libavcodec.so"));
		swscale_lib_ptr = LibraryPtr(new Utility::Library("libswscale.so"));
		avutil_lib_ptr = LibraryPtr(new Utility::Library("libavutil.so"));
		avformat_lib_ptr = LibraryPtr(new Utility::Library("libavformat.so"));
		avfilter_lib_ptr = LibraryPtr(new Utility::Library("libavfilter.so"));
#endif
		_avcodec_find_encoder = (f_avcodec_find_encoder)avcodec_lib_ptr->GetSym("avcodec_find_encoder");
		if (!_avcodec_find_encoder)
			throw std::runtime_error("avcodec_find_encoder not found");

		_avcodec_find_decoder = (f_avcodec_find_decoder)avcodec_lib_ptr->GetSym("avcodec_find_decoder");
		if (!_avcodec_find_decoder)
			throw std::runtime_error("avcodec_find_decoder not found");


		_avcodec_alloc_frame = (f_avcodec_alloc_frame)avcodec_lib_ptr->GetSym("avcodec_alloc_frame");
		if (!_avcodec_alloc_frame)
			throw std::runtime_error("avcodec_alloc_frame not found");

		_avcodec_open2 = (f_avcodec_open2)avcodec_lib_ptr->GetSym("avcodec_open2");
		if (!_avcodec_open2)
			throw std::runtime_error("avcodec_open2 not found");

		_avcodec_close = (f_avcodec_close)avcodec_lib_ptr->GetSym("avcodec_close");
		if (!_avcodec_close)
			throw std::runtime_error("_avcodec_close not found");


		_avcodec_alloc_context3 = (f_avcodec_alloc_context3)avcodec_lib_ptr->GetSym("avcodec_alloc_context3");
		if (!_avcodec_alloc_context3)
			throw std::runtime_error("avcodec_alloc_context3 not found");

		_avcodec_register_all = (f_avcodec_register_all)avcodec_lib_ptr->GetSym("avcodec_register_all");
		if (!_avcodec_register_all)
			throw std::runtime_error("avcodec_register_all not found");

		_avcodec_encode_video2 = (f_avcodec_encode_video2)avcodec_lib_ptr->GetSym("avcodec_encode_video2");
		if (!_avcodec_encode_video2)
			throw std::runtime_error("avcodec_encode_video2 not found");

		_avcodec_decode_video2 = (f_avcodec_decode_video2)avcodec_lib_ptr->GetSym("avcodec_decode_video2");
		if (!_avcodec_decode_video2)
			throw std::runtime_error("avcodec_decode_video2 not found");

		_avcodec_get_name = (f_avcodec_get_name)avcodec_lib_ptr->GetSym("avcodec_get_name");
		if (!_avcodec_get_name)
			throw std::runtime_error("avcodec_get_name not found");


		_avpicture_fill = (f_avpicture_fill)avcodec_lib_ptr->GetSym("avpicture_fill");
		if (!_avpicture_fill)
			throw std::runtime_error("avpicture_fill not found");

		_av_picture_copy = (f_av_picture_copy)avcodec_lib_ptr->GetSym("av_picture_copy");
		if (!_av_picture_copy)
			throw std::runtime_error("av_picture_copy not found");


		_av_init_packet = (f_av_init_packet)avcodec_lib_ptr->GetSym("av_init_packet");
		if (!_av_init_packet)
			throw std::runtime_error("av_init_packet not found");

		_av_free_packet = (f_av_free_packet)avcodec_lib_ptr->GetSym("av_free_packet");
		if (!_av_free_packet)
			throw std::runtime_error("av_free_packet not found");


		_avpicture_alloc = (f_avpicture_alloc)avcodec_lib_ptr->GetSym("avpicture_alloc");
		if (!_avpicture_alloc)
			throw std::runtime_error("avpicture_alloc not found");

		_avpicture_layout = (f_avpicture_layout)avcodec_lib_ptr->GetSym("avpicture_layout");
		if (!_avpicture_layout)
			throw std::runtime_error("avpicture_layout not found");

		_avpicture_free = (f_avpicture_free)avcodec_lib_ptr->GetSym("avpicture_free");
		if (!_avpicture_free)
			throw std::runtime_error("avpicture_free not found");


		_sws_getContext = (f_sws_getContext)swscale_lib_ptr->GetSym("sws_getContext");
		if (!_sws_getContext)
			throw std::runtime_error("sws_getContext not found");

		_sws_scale = (f_sws_scale)swscale_lib_ptr->GetSym("sws_scale");
		if (!_sws_scale)
			throw std::runtime_error("sws_scale not found");

		_sws_freeContext = (f_sws_freeContext)swscale_lib_ptr->GetSym("sws_freeContext");
		if (!_sws_freeContext)
			throw std::runtime_error("sws_freeContext not found");

		_av_frame_alloc = (f_av_frame_alloc)avutil_lib_ptr->GetSym("av_frame_alloc");
		if (!_av_frame_alloc)
			throw std::runtime_error("av_frame_alloc not found");

		_av_frame_free = (f_av_frame_free)avutil_lib_ptr->GetSym("av_frame_free");
		if (!_av_frame_free)
			throw std::runtime_error("av_frame_free not found");

		_av_frame_unref = (f_av_frame_unref)avutil_lib_ptr->GetSym("av_frame_unref");
		if (!_av_frame_unref)
			throw std::runtime_error("av_frame_unref not found");

		_av_frame_clone = (f_av_frame_clone)avutil_lib_ptr->GetSym("av_frame_clone");
		if (!_av_frame_clone)
			throw std::runtime_error("av_frame_clone not found");

		_av_frame_get_buffer = (f_av_frame_get_buffer)avutil_lib_ptr->GetSym("av_frame_get_buffer");
		if (!_av_frame_get_buffer)
			throw std::runtime_error("av_frame_get_buffer not found");


		_av_rescale_q_rnd = (f_av_rescale_q_rnd)avutil_lib_ptr->GetSym("av_rescale_q_rnd");
		if (!_av_rescale_q_rnd)
			throw std::runtime_error("av_rescale_q_rnd not found");

		_av_rescale_q = (f_av_rescale_q)avutil_lib_ptr->GetSym("av_rescale_q");
		if (!_av_rescale_q)
			throw std::runtime_error("av_rescale_q not found");

		_av_dict_set = (f_av_dict_set)avutil_lib_ptr->GetSym("av_dict_set");
		if (!_av_dict_set)
			throw std::runtime_error("av_dict_set not found");

		_av_dict_count = (f_av_dict_count)avutil_lib_ptr->GetSym("av_dict_count");
		if (!_av_dict_count)
			throw std::runtime_error("av_dict_count not found");

		_av_get_pix_fmt_name = (f_av_get_pix_fmt_name)avutil_lib_ptr->GetSym("av_get_pix_fmt_name");
		if (!_av_get_pix_fmt_name)
			throw std::runtime_error("av_get_pix_fmt_name not found");



		_av_free = (f_av_free)avutil_lib_ptr->GetSym("av_free");
		if (!_av_free)
			throw std::runtime_error("av_free not found");

		_av_freep = (f_av_freep)avutil_lib_ptr->GetSym("av_freep");
		if (!_av_freep)
			throw std::runtime_error("av_freep not found");

		_av_opt_set = (f_av_opt_set)avutil_lib_ptr->GetSym("av_opt_set");
		if (!_av_opt_set)
			throw std::runtime_error("av_opt_set not found");

		_av_strdup = (f_av_strdup)avutil_lib_ptr->GetSym("av_strdup");
		if (!_av_strdup)
			throw std::runtime_error("av_strdup not found");

		_av_int_list_length_for_size  = (f_av_int_list_length_for_size )avutil_lib_ptr->GetSym("av_int_list_length_for_size");
		if (!_av_int_list_length_for_size )
			throw std::runtime_error("av_int_list_length_for_size  not found");

		_av_opt_set_bin  = (f_av_opt_set_bin)avutil_lib_ptr->GetSym("av_opt_set_bin");
		if (!_av_opt_set_bin)
			throw std::runtime_error("av_opt_set_bin not found");



		_avformat_alloc_output_context2 = (f_avformat_alloc_output_context2)avformat_lib_ptr->GetSym("avformat_alloc_output_context2");
		if (!_avformat_alloc_output_context2)
			throw std::runtime_error("avformat_alloc_output_context2 not found");

		_av_register_all = (f_av_register_all)avformat_lib_ptr->GetSym("av_register_all");
		if (!_av_register_all)
			throw std::runtime_error("av_register_all not found");

		_avformat_new_stream = (f_avformat_new_stream)avformat_lib_ptr->GetSym("avformat_new_stream");
		if (!_avformat_new_stream)
			throw std::runtime_error("avformat_new_stream not found");

		_avformat_free_context = (f_avformat_free_context)avformat_lib_ptr->GetSym("avformat_free_context");
		if (!_avformat_free_context)
			throw std::runtime_error("avformat_free_context not found");


		_avio_alloc_context = (f_avio_alloc_context)avformat_lib_ptr->GetSym("avio_alloc_context");
		if (!_avio_alloc_context)
			throw std::runtime_error("avio_alloc_context not found");

		_avio_open = (f_avio_open)avformat_lib_ptr->GetSym("avio_open");
		if (!_avio_open)
			throw std::runtime_error("avio_open not found");

		_av_write_frame = (f_av_write_frame)avformat_lib_ptr->GetSym("av_write_frame");
		if (!_av_write_frame)
			throw std::runtime_error("av_write_frame not found");

		_av_interleaved_write_frame = (f_av_interleaved_write_frame)avformat_lib_ptr->GetSym("av_interleaved_write_frame");
		if (!_av_interleaved_write_frame)
			throw std::runtime_error("av_interleaved_write_frame not found");

		_avformat_write_header = (f_avformat_write_header)avformat_lib_ptr->GetSym("avformat_write_header");
		if (!_avformat_write_header)
			throw std::runtime_error("avformat_write_header not found");

		_av_write_trailer = (f_av_write_trailer)avformat_lib_ptr->GetSym("av_write_trailer");
		if (!_av_write_trailer)
			throw std::runtime_error("av_write_trailer not found");

		_avfilter_register_all = (f_avfilter_register_all)avfilter_lib_ptr->GetSym("avfilter_register_all");
		if (!_avfilter_register_all)
			throw std::runtime_error("avfilter_register_all not found");		

		_avfilter_get_by_name = (f_avfilter_get_by_name)avfilter_lib_ptr->GetSym("avfilter_get_by_name");
		if (!_avfilter_get_by_name)
			throw std::runtime_error("avfilter_get_by_name not found");

		_avfilter_inout_alloc = (f_avfilter_inout_alloc)avfilter_lib_ptr->GetSym("avfilter_inout_alloc");
		if (!_avfilter_inout_alloc)
			throw std::runtime_error("avfilter_inout_alloc not found");

		_avfilter_inout_free = (f_avfilter_inout_free)avfilter_lib_ptr->GetSym("avfilter_inout_free");
		if (!_avfilter_inout_free)
			throw std::runtime_error("avfilter_inout_free not found");

		_avfilter_graph_alloc = (f_avfilter_graph_alloc)avfilter_lib_ptr->GetSym("avfilter_graph_alloc");
		if (!_avfilter_graph_alloc)
			throw std::runtime_error("avfilter_graph_alloc not found");

		_avfilter_graph_create_filter = (f_avfilter_graph_create_filter)avfilter_lib_ptr->GetSym("avfilter_graph_create_filter");
		if (!_avfilter_graph_create_filter)
			throw std::runtime_error("avfilter_graph_create_filter not found");

		_avfilter_graph_parse_ptr = (f_avfilter_graph_parse_ptr)avfilter_lib_ptr->GetSym("avfilter_graph_parse_ptr");
		if (!_avfilter_graph_parse_ptr)
			throw std::runtime_error("avfilter_graph_parse_ptr not found");

		_avfilter_graph_config = (f_avfilter_graph_config)avfilter_lib_ptr->GetSym("avfilter_graph_config");
		if (!_avfilter_graph_config)
			throw std::runtime_error("avfilter_graph_config not found");

		_av_buffersrc_add_frame_flags = (f_av_buffersrc_add_frame_flags)avfilter_lib_ptr->GetSym("av_buffersrc_add_frame_flags");
		if (!_av_buffersrc_add_frame_flags)
			throw std::runtime_error("av_buffersrc_add_frame_flags not found");

		_av_buffersink_get_frame_flags = (f_av_buffersink_get_frame_flags)avfilter_lib_ptr->GetSym("av_buffersink_get_frame_flags");
		if (!_av_buffersink_get_frame_flags)
			throw std::runtime_error("av_buffersink_get_frame_flags not found");

		_av_buffersink_get_frame = (f_av_buffersink_get_frame)avfilter_lib_ptr->GetSym("av_buffersink_get_frame");
		if (!_av_buffersink_get_frame)
			throw std::runtime_error("av_buffersink_get_frame not found");

		_avfilter_graph_free = (f_avfilter_graph_free)avfilter_lib_ptr->GetSym("avfilter_graph_free");
		if (!_avfilter_graph_free)
			throw std::runtime_error("avfilter_graph_free not found");


		_av_register_all();
		avfilter_register_all();

		std::cout<<"libav* loaded ok"<<std::endl;

		return true;
	}
	catch(std::runtime_error ex){
		error_message = std::string(ex.what());
	}
	return false;

}

AVCodec * libav::avcodec_find_encoder(enum AVCodecID id){
	if (!_avcodec_find_encoder)
	throw std::runtime_error("avcodec_find_encoder not found");
	return _avcodec_find_encoder(id);
};

AVCodec *libav::avcodec_find_decoder(enum AVCodecID id){
	if (!_avcodec_find_decoder)
	throw std::runtime_error("avcodec_find_decoder not found");
	return _avcodec_find_decoder(id);

};

AVFrame *libav::avcodec_alloc_frame(void){
	if (!_avcodec_alloc_frame)
		throw std::runtime_error("avcodec_alloc_frame not found");
	return _avcodec_alloc_frame();

}

int libav::avcodec_open2(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options){
	if (!_avcodec_open2)
		throw std::runtime_error("avcodec_open2 not found");
	return _avcodec_open2(avctx,codec,options);

}

int libav::avcodec_close(AVCodecContext *avctx){
	if (!_avcodec_close)
		throw std::runtime_error("avcodec_close not found");
	return _avcodec_close(avctx);
}


AVCodecContext *libav::avcodec_alloc_context3(const AVCodec *codec){
	if (!_avcodec_alloc_context3)
		throw std::runtime_error("avcodec_alloc_context3 not found");
	return _avcodec_alloc_context3(codec);

}

int libav::avcodec_encode_video2(AVCodecContext *avctx, AVPacket *avpkt,
	const AVFrame *frame, int *got_packet_ptr){
		if (!_avcodec_encode_video2)
			throw std::runtime_error("avcodec_encode_video2 not found");
		return _avcodec_encode_video2(avctx,avpkt,frame,got_packet_ptr);
}

int libav::avcodec_decode_video2(AVCodecContext *avctx, AVFrame *picture,
	int *got_picture_ptr,
	const AVPacket *avpkt){
		if (!_avcodec_decode_video2)
			throw std::runtime_error("avcodec_decode_video2 not found");
		return _avcodec_decode_video2(avctx,picture,got_picture_ptr,avpkt);

}

const char *libav::avcodec_get_name(enum AVCodecID id){
	if (!_avcodec_get_name)
	throw std::runtime_error("avcodec_get_name not found");
	return _avcodec_get_name(id);
};

int libav::avpicture_fill(AVPicture *picture, const uint8_t *ptr,
	enum AVPixelFormat pix_fmt, int width, int height){
		if (!_avpicture_fill)
		throw std::runtime_error("avpicture_fill not found");
		return _avpicture_fill(picture,ptr,pix_fmt,width,height);

};

void libav::av_picture_copy(AVPicture *dst, const AVPicture *src,
	enum AVPixelFormat pix_fmt, int width, int height){
		if (!_av_picture_copy)
		throw std::runtime_error("av_picture_copy not found");
		return _av_picture_copy(dst,src,pix_fmt,width,height);

};


void libav::av_init_packet(AVPacket* pkt){
	if (!_av_init_packet)
		throw std::runtime_error("av_init_packet not found");
	return _av_init_packet(pkt);

}

void libav::av_free_packet(AVPacket *pkt){
	if (!_av_free_packet)
		throw std::runtime_error("av_free_packet not found");
	return _av_free_packet(pkt);
}

int libav::avpicture_alloc(AVPicture *picture, enum AVPixelFormat pix_fmt, int width, int height){
	if (!_avpicture_alloc)
	throw std::runtime_error("avpicture_alloc not found");
	return _avpicture_alloc(picture,pix_fmt,width,height);
};

int libav::avpicture_layout(const AVPicture *src, enum AVPixelFormat pix_fmt,
	int width, int height,
	unsigned char *dest, int dest_size){
		if (!_avpicture_layout)
			throw std::runtime_error("avpicture_layout not found");
		return _avpicture_layout(src,pix_fmt,width,height,dest,dest_size);

}

void libav::avpicture_free(AVPicture *picture){
	if (!_avpicture_free)
		throw std::runtime_error("avpicture_free not found");
	return _avpicture_free(picture);

}

struct SwsContext *libav::sws_getContext(int srcW, int srcH, enum AVPixelFormat srcFormat,
	int dstW, int dstH, enum AVPixelFormat dstFormat,
	int flags, SwsFilter *srcFilter,
	SwsFilter *dstFilter, const double *param){
		if (!_sws_getContext)
			throw std::runtime_error("sws_getContext not found");
		return _sws_getContext(srcW,srcH,srcFormat,dstW,dstH, dstFormat, flags, srcFilter, dstFilter,param);

}

int libav::sws_scale(struct SwsContext *c, const uint8_t *const srcSlice[],
	const int srcStride[], int srcSliceY, int srcSliceH,
	uint8_t *const dst[], const int dstStride[]){
		if (!_sws_scale)
			throw std::runtime_error("sws_scale not found");
		return _sws_scale(c,srcSlice,srcStride, srcSliceY, srcSliceH,dst, dstStride);

}

void libav::sws_freeContext(struct SwsContext *swsContext){
	if (!_sws_freeContext)
		throw std::runtime_error("sws_freeContext not found");
	return _sws_freeContext(swsContext);
}


void libav::av_free(void *ptr){
	if (!_av_free)
		throw std::runtime_error("av_free not found");
	return _av_free(ptr);
}

void libav::av_freep(void *ptr){
	if (!_av_freep)
		throw std::runtime_error("av_freep not found");
	return _av_freep(ptr);

}

AVFrame* libav::av_frame_alloc(void){
	if (!_av_frame_alloc)
		throw std::runtime_error("av_frame_alloc not found");
	return _av_frame_alloc();

}

void libav::av_frame_free(AVFrame **frame){
	if (!_av_frame_free)
		throw std::runtime_error("av_frame_free not found");
	return _av_frame_free(frame);
}

void libav::av_frame_unref(AVFrame *frame){
	if (!_av_frame_unref)
		throw std::runtime_error("av_frame_unref not found");
	return _av_frame_unref(frame);

}
AVFrame *libav::av_frame_clone(const AVFrame *src){
	if (!_av_frame_clone)
		throw std::runtime_error("av_frame_clone not found");
	return _av_frame_clone(src);

}



int libav::av_frame_get_buffer(AVFrame *frame, int align){
	if (!_av_frame_get_buffer)
		throw std::runtime_error("av_frame_get_buffer not found");
	return _av_frame_get_buffer(frame,align);

}

int64_t libav::av_rescale_q_rnd(int64_t a, AVRational bq, AVRational cq, enum AVRounding e){
	if (!_av_rescale_q_rnd)
	throw std::runtime_error("av_rescale_q_rnd not found");
	return _av_rescale_q_rnd(a, bq, cq, e);

};
int64_t libav::av_rescale_q(int64_t a, AVRational bq, AVRational cq){
	if (!_av_rescale_q)
		throw std::runtime_error("av_rescale_q not found");
	return _av_rescale_q(a, bq, cq);

}

int libav::av_dict_set(AVDictionary **pm, const char *key, const char *value, int flags){
	if (!_av_dict_set)
		throw std::runtime_error("av_dict_set not found");
	return _av_dict_set(pm, key, value, flags);
}
int libav::av_dict_count(const AVDictionary *m){
	if (!_av_dict_count)
		throw std::runtime_error("av_dict_count not found");
	return _av_dict_count(m);
}


const char *libav::av_get_pix_fmt_name(enum AVPixelFormat pix_fmt){
	if (!_av_get_pix_fmt_name)
	throw std::runtime_error("av_get_pix_fmt_name not found");
	return _av_get_pix_fmt_name(pix_fmt);

};


int libav::av_opt_set(void *obj, const char *name, const char *val, int search_flags){
	if (!_av_opt_set)
		throw std::runtime_error("av_opt_set not found");
	return _av_opt_set(obj,name,val,search_flags);

}

char* libav::av_strdup(const char *s){
	if (!_av_strdup)
		throw std::runtime_error("av_strdup not found");
	return _av_strdup(s);

}
unsigned int libav::av_int_list_length_for_size(unsigned int elsize,
	const void *list, uint64_t term){
		if (!_av_int_list_length_for_size)
			throw std::runtime_error("av_int_list_length_for_size not found");
		return _av_int_list_length_for_size(elsize,list,term);

}
int libav::av_opt_set_bin(void *obj, const char *name, const uint8_t *val, int size, int search_flags){
	if (!_av_opt_set_bin)
		throw std::runtime_error("av_opt_set_bin not found");
	return _av_opt_set_bin(obj,name,val,size,search_flags);

}



int libav::avformat_alloc_output_context2(AVFormatContext **ctx, AVOutputFormat *oformat,
	const char *format_name, const char *filename){
		if (!_avformat_alloc_output_context2)
			throw std::runtime_error("avformat_alloc_output_context2 not found");
		return _avformat_alloc_output_context2(ctx,oformat,format_name,filename);
}

void libav::avformat_free_context(AVFormatContext *s){
	if (!_avformat_free_context)
		throw std::runtime_error("avformat_free_context not found");
	return _avformat_free_context(s);
}

AVStream *libav::avformat_new_stream(AVFormatContext *s, const AVCodec *c){
	if (!_avformat_new_stream)
		throw std::runtime_error("avformat_new_stream not found");
	return _avformat_new_stream(s,c);

}

AVIOContext *libav::avio_alloc_context(
	unsigned char *buffer,
	int buffer_size,
	int write_flag,
	void *opaque,
	int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
	int (*write_packet)(void *opaque, uint8_t *buf, int buf_size),
	int64_t (*seek)(void *opaque, int64_t offset, int whence)){
		if (!_avio_alloc_context)
			throw std::runtime_error("avio_alloc_context not found");
		return _avio_alloc_context(buffer,buffer_size,write_flag,opaque,read_packet,write_packet,seek);

}

int libav::avio_open(AVIOContext **s, const char *url, int flags){
	if (!_avio_open)
		throw std::runtime_error("avio_open not found");
	return _avio_open(s,url,flags);

}

int libav::av_write_frame(AVFormatContext *s, AVPacket *pkt){
	if (!_av_write_frame)
		throw std::runtime_error("av_write_frame not found");
	return _av_write_frame(s,pkt);

}

int libav::av_interleaved_write_frame(AVFormatContext *s, AVPacket *pkt){
	if (!_av_interleaved_write_frame)
		throw std::runtime_error("av_interleaved_write_frame not found");
	return _av_interleaved_write_frame(s,pkt);
}

int libav::avformat_write_header(AVFormatContext *s, AVDictionary **options){
	if (!_avformat_write_header)
		throw std::runtime_error("avformat_write_header not found");
	return _avformat_write_header(s,options);
}

int libav::av_write_trailer(AVFormatContext *s){
	if (!_av_write_trailer)
		throw std::runtime_error("av_write_trailer not found");
	return _av_write_trailer(s);
}


void libav::avfilter_register_all(void){
	if (!_avfilter_register_all)
		throw std::runtime_error("avfilter_register_all not found");
	return _avfilter_register_all();

}

AVFilter *libav::avfilter_get_by_name(const char *name){
	if (!_avfilter_get_by_name)
		throw std::runtime_error("avfilter_get_by_name not found");
	return _avfilter_get_by_name(name);

}
AVFilterInOut *libav::avfilter_inout_alloc(void){
	if (!_avfilter_inout_alloc)
		throw std::runtime_error("avfilter_inout_alloc not found");
	return _avfilter_inout_alloc();

}

void libav::avfilter_inout_free(AVFilterInOut **inout){
	if (!_avfilter_inout_free)
		throw std::runtime_error("avfilter_inout_free not found");
	return _avfilter_inout_free(inout);
}

AVFilterGraph *libav::avfilter_graph_alloc(void){
	if (!_avfilter_graph_alloc)
		throw std::runtime_error("avfilter_graph_alloc not found");
	return _avfilter_graph_alloc();

}
int libav::avfilter_graph_create_filter(AVFilterContext **filt_ctx, const AVFilter *filt,
	const char *name, const char *args, void *opaque,
	AVFilterGraph *graph_ctx){
		if (!_avfilter_graph_create_filter)
			throw std::runtime_error("avfilter_graph_create_filter not found");
		return _avfilter_graph_create_filter(filt_ctx, filt,name, args,opaque,graph_ctx);
}
int libav::avfilter_graph_parse_ptr(AVFilterGraph *graph, const char *filters,
	AVFilterInOut **inputs, AVFilterInOut **outputs,
	void *log_ctx){
		if (!_avfilter_graph_parse_ptr)
			throw std::runtime_error("avfilter_graph_parse_ptr not found");
		return _avfilter_graph_parse_ptr(graph, filters,inputs, outputs,log_ctx);
}

int libav::avfilter_graph_config(AVFilterGraph *graphctx, void *log_ctx){
	if (!_avfilter_graph_config)
		throw std::runtime_error("avfilter_graph_config not found");
	return _avfilter_graph_config(graphctx, log_ctx);

}

int libav::av_buffersrc_add_frame_flags(AVFilterContext *buffer_src,
	AVFrame *frame, int flags){
		if (!_av_buffersrc_add_frame_flags)
			throw std::runtime_error("av_buffersrc_add_frame_flags not found");
		return _av_buffersrc_add_frame_flags(buffer_src,frame, flags);
}
int libav::av_buffersink_get_frame_flags(AVFilterContext *ctx, AVFrame *frame, int flags){
	if (!_av_buffersink_get_frame_flags)
		throw std::runtime_error("av_buffersink_get_frame_flags not found");
	return _av_buffersink_get_frame_flags(ctx,frame, flags);
}

int libav::av_buffersink_get_frame(AVFilterContext *ctx, AVFrame *frame){
	if (!_av_buffersink_get_frame)
		throw std::runtime_error("av_buffersink_get_frame not found");
	return _av_buffersink_get_frame(ctx,frame);
}

void libav::avfilter_graph_free(AVFilterGraph **graph){
	if (!_avfilter_graph_free)
		throw std::runtime_error("avfilter_graph_free not found");
	return _avfilter_graph_free(graph);

}


