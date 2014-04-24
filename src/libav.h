#ifndef __libav_h__
#define __libav_h__


#include <string>

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/mathematics.h>
#include <libavutil/pixdesc.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>

class libav{
public:
	static bool initialize(std::string& error_message);

	//libavcodec
	static AVCodec *avcodec_find_encoder(enum AVCodecID id);
	static AVCodec *avcodec_find_decoder(enum AVCodecID id);

	static AVCodecContext *avcodec_alloc_context3(const AVCodec *codec);
	static AVFrame *avcodec_alloc_frame(void);
	static int avpicture_alloc(AVPicture *picture, enum AVPixelFormat pix_fmt, int width, int height);
	static void avpicture_free(AVPicture *picture);

	static int avcodec_open2(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);
	static int avcodec_close(AVCodecContext *avctx);
	static void avcodec_register_all(void);

	static int avcodec_encode_video2(AVCodecContext *avctx, AVPacket *avpkt,
		const AVFrame *frame, int *got_packet_ptr);
	static int avcodec_decode_video2(AVCodecContext *avctx, AVFrame *picture,
		int *got_picture_ptr,
		const AVPacket *avpkt);

	static const char *avcodec_get_name(enum AVCodecID id);


	static int avpicture_fill(AVPicture *picture, const uint8_t *ptr,
		enum AVPixelFormat pix_fmt, int width, int height);

	static void av_picture_copy(AVPicture *dst, const AVPicture *src,
		enum AVPixelFormat pix_fmt, int width, int height);

	static int avpicture_layout(const AVPicture *src, enum AVPixelFormat pix_fmt,
		int width, int height,
		unsigned char *dest, int dest_size);


	static void av_init_packet(AVPacket* pkt);
	static void av_free_packet(AVPacket *pkt);


	//libswscale
	static struct SwsContext *sws_getContext(int srcW, int srcH, enum AVPixelFormat srcFormat,
		int dstW, int dstH, enum AVPixelFormat dstFormat,
		int flags, SwsFilter *srcFilter,
		SwsFilter *dstFilter, const double *param);

	static int sws_scale(struct SwsContext *c, const uint8_t *const srcSlice[],
		const int srcStride[], int srcSliceY, int srcSliceH,
		uint8_t *const dst[], const int dstStride[]);

	static void sws_freeContext(struct SwsContext *swsContext);


	//libavutil
	static void av_free(void *ptr);
	static void av_freep(void *ptr);
	static AVFrame *av_frame_alloc(void);
	static void av_frame_free(AVFrame **frame);
	static void av_frame_unref(AVFrame *frame);
	static AVFrame *av_frame_clone(const AVFrame *src);
	static int av_frame_get_buffer(AVFrame *frame, int align);
	static int64_t av_rescale_q_rnd(int64_t a, AVRational bq, AVRational cq, enum AVRounding);
	static int64_t av_rescale_q(int64_t a, AVRational bq, AVRational cq);
	static int av_dict_set(AVDictionary **pm, const char *key, const char *value, int flags);
	static int av_dict_count(const AVDictionary *m);
	static const char *av_get_pix_fmt_name(enum AVPixelFormat pix_fmt);
	static int av_opt_set       (void *obj, const char *name, const char *val, int search_flags);
	static char *av_strdup(const char *s);
	static unsigned int av_int_list_length_for_size(unsigned int elsize,
		const void *list, uint64_t term) av_pure;
	static int av_opt_set_bin   (void *obj, const char *name, const uint8_t *val, int size, int search_flags);



	//libavformat
	static void av_register_all(void);
	static int avformat_alloc_output_context2(AVFormatContext **ctx, AVOutputFormat *oformat,
		const char *format_name, const char *filename);
	static AVStream *avformat_new_stream(AVFormatContext *s, const AVCodec *c);
	static void avformat_free_context(AVFormatContext *s);

	static AVIOContext *avio_alloc_context(
		unsigned char *buffer,
		int buffer_size,
		int write_flag,
		void *opaque,
		int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
		int (*write_packet)(void *opaque, uint8_t *buf, int buf_size),
		int64_t (*seek)(void *opaque, int64_t offset, int whence));
	static int avio_open(AVIOContext **s, const char *url, int flags);
	static int av_write_frame(AVFormatContext *s, AVPacket *pkt);
	static int avformat_write_header(AVFormatContext *s, AVDictionary **options);


	//libavfilter
	static void avfilter_register_all(void);
	static AVFilter *avfilter_get_by_name(const char *name);
	static AVFilterInOut *avfilter_inout_alloc(void);
	static void avfilter_inout_free(AVFilterInOut **inout);
	static AVFilterGraph *avfilter_graph_alloc(void);
	static int avfilter_graph_create_filter(AVFilterContext **filt_ctx, const AVFilter *filt,
		const char *name, const char *args, void *opaque,
		AVFilterGraph *graph_ctx);
	static int avfilter_graph_parse_ptr(AVFilterGraph *graph, const char *filters,
		AVFilterInOut **inputs, AVFilterInOut **outputs,
		void *log_ctx);
	static int avfilter_graph_config(AVFilterGraph *graphctx, void *log_ctx);
	static int av_buffersrc_add_frame_flags(AVFilterContext *buffer_src,
		AVFrame *frame, int flags);
	static int av_buffersink_get_frame_flags(AVFilterContext *ctx, AVFrame *frame, int flags);
	static int av_buffersink_get_frame(AVFilterContext *ctx, AVFrame *frame);
	static void avfilter_graph_free(AVFilterGraph **graph);
};


#endif//__libav_h__