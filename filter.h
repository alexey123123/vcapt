#ifndef __Filter_h__
#define __Filter_h__

#include <boost/shared_ptr.hpp>

#include "libav.h"

class filter{
public:
	filter():

			//filter stuff
			buffersink_ctx(0),
			buffersrc_ctx(0),
			buffersrc(0),
			buffersink(0),
			outputs(0),
			inputs(0),
			filter_graph(0)
			{};
	~filter();

	AVFrame* apply_to_frame(AVFrame* src, const std::string& filter_string);//throw std::runtime_error


	void clean();
private:
	std::string last_filter_string;
	std::string last_filter_args;
	

	AVFilterContext *buffersink_ctx;
	AVFilterContext *buffersrc_ctx;
	AVFilter *buffersrc;
	AVFilter *buffersink;
	AVFilterInOut *outputs;
	AVFilterInOut *inputs;
	AVFilterGraph *filter_graph;

};

typedef boost::shared_ptr<filter> filter_ptr;

#endif//__Filter_h__