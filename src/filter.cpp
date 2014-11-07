#include <stdexcept>


#include "filter.h"
#include "types.h"


#define __av_opt_set_int_list(obj, name, val, term, flags) \
	(libav::av_int_list_length(val, term) > INT_MAX / sizeof(*(val)) ? \
	AVERROR(EINVAL) : \
	libav::av_opt_set_bin(obj, name, (const uint8_t *)(val), \
	libav::av_int_list_length(val, term) * sizeof(*(val)), flags))

AVFrame* filter::draw_text_in_center(AVFrame* src, const std::string& text_string){
	return apply_to_frame(src,"drawtext=x=(w*0.5 - text_w*0.5):y=(h*0.5-line_h*0.5):fontsize=20:fontcolor=white:text='"+text_string+"':fontfile="+FontsPath+"couri.ttf:box=1:boxcolor=black");
}

AVFrame* filter::apply_to_frame(AVFrame* src, const std::string& filter_string){


	if (filter_string.size()==0)
		return libav::av_frame_clone(src);


	char args[255];
	memset(args,0,255);
	sprintf(args,
		"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		//"video_size=%dx%d:pix_fmt=%d",
		src->width, src->height, src->format
		,1, 1,
		//src->sample_aspect_ratio.num, src->sample_aspect_ratio.den
		src->width, src->height
		);
	if ((std::string(args) != last_filter_args)||(filter_string != last_filter_string)){
		clean();
		buffersrc  = libav::avfilter_get_by_name("buffer");
		buffersink = libav::avfilter_get_by_name("buffersink");
		outputs = libav::avfilter_inout_alloc();
		inputs  = libav::avfilter_inout_alloc();
		enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };

		filter_graph = libav::avfilter_graph_alloc();
		if (!outputs || !inputs || !filter_graph) 
			throw std::runtime_error("no memory");

		/* buffer video source: the decoded frames from the decoder will be inserted here. */
		AVRational r1;
		r1.num = 0;
		r1.den = 0;
		int ret = libav::avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
			args, NULL, filter_graph);
		if (ret < 0) 
			throw std::runtime_error("Cannot create buffer source");
		

		/* buffer video sink: to terminate the filter chain. */
		ret = libav::avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
			NULL, NULL, filter_graph);
		if (ret < 0)
			throw std::runtime_error("Cannot create buffer sink");

		ret = __av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
			AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
		if (ret < 0)
			throw std::runtime_error("Cannot set output pixel format");

		/* Endpoints for the filter graph. */
		outputs->name       = libav::av_strdup("in");
		outputs->filter_ctx = buffersrc_ctx;
		outputs->pad_idx    = 0;
		outputs->next       = NULL;

		inputs->name       = libav::av_strdup("out");
		inputs->filter_ctx = buffersink_ctx;
		inputs->pad_idx    = 0;
		inputs->next       = NULL;


		if ((ret = libav::avfilter_graph_parse_ptr(filter_graph,			
			//"noise=alls=20:allf=t+u,drawtext=x=(w - text_w - w*0.02):y=(h-text_h-line_h):fontsize=20:fontcolor=white:text=%{localtime}:fontfile=couri.ttf",
			(const char*)filter_string.c_str(),
			&inputs, &outputs, NULL)) < 0)
				throw std::runtime_error("filter parse error");

		if ((ret = libav::avfilter_graph_config(filter_graph, NULL)) < 0)
			throw std::runtime_error("avfilter_graph_config error");
	}


	if (libav::av_buffersrc_add_frame_flags(buffersrc_ctx, src, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) 
		throw std::runtime_error("av_buffersrc_add_frame_flags error");
	
	AVFrame *filt_frame = libav::av_frame_alloc();
	if (libav::av_buffersink_get_frame(buffersink_ctx, filt_frame)<0)
		throw std::runtime_error("av_buffersink_get_frame error");

	filt_frame->pts = src->pts;


	return filt_frame;

}


filter::~filter(){
	clean();
}

void filter::clean(){
	libav::avfilter_inout_free(&inputs);
	inputs = 0;

	libav::avfilter_inout_free(&outputs);
	outputs = 0;

	libav::avfilter_graph_free(&filter_graph);
	filter_graph = 0;
}