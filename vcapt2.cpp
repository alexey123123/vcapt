#include <iostream>
#include <string>

#include <utility/Journal.h>
#include <utility/Options.h>

#include "manager.h"
#include "libav.h"


int main(int argc,char** argv){

	using namespace Utility;
	Journal* journal = Journal::Instance("vcapt",DST_STDOUT|DST_SYSLOG);




	try{
		boost::program_options::options_description desc("vcapt");
		Options* opts = Options::Instance(argc,argv,"vcapt [options]",desc);
		if (!opts)
			throw std::runtime_error("options error");

		std::string error_message;
		if (!libav::initialize(error_message))
			throw std::runtime_error("libav error: "+error_message);



		manager _manager(opts);


		std::string sss;
		std::cin>>sss;

	}
	catch(std::runtime_error& ex){
		journal->Write(ERR,DST_SYSLOG|DST_STDERR,"runtime error:%s",ex.what());
	}



	return 1;
}