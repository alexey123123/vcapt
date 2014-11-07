#include <boost/bind.hpp>

#include <utility/Journal.h>
#include <system/Platform.h>

#include "archive_client.h"
#include "types.h"

archive_client::archive_client(couchdb::manager& _cdb_manager):cdb_manager(_cdb_manager){


	internal_thread = boost::thread(boost::bind(&archive_client::internal_thread_proc,this));
}


archive_client::~archive_client(){
	internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>();	
	internal_thread.join();

}


void archive_client::internal_thread_proc(){
	try{
		internal_ioservice_work_ptr = boost::shared_ptr<boost::asio::io_service::work>(
			new boost::asio::io_service::work(internal_ioservice));
		internal_ioservice.run();
	}
	catch(...){
		using namespace Utility;
		Journal::Instance()->Write(ALERT,DST_STDERR|DST_SYSLOG,"unhandled exception in archive_client::internal_thread");
	}

}


void archive_client::create_long_writing_file(
	file_type ft, 
	boost::int64_t assumed_file_size,
	const std::string& author_name,		//camera name or anything else...
	create_long_writing_file_callback cb){
		internal_ioservice.post(boost::bind(&archive_client::i_thread_create_long_writing_file,this,
			ft,assumed_file_size,author_name,cb));
}

static int __fcount__ = 0;

void archive_client::i_thread_create_long_writing_file(
	file_type ft, 
	boost::int64_t assumed_file_size,
	const std::string& author_name,		//camera name or anything else...
	create_long_writing_file_callback cb){

		//TODO: http-request to archive server


		long_writing_file_ptr lf_ptr(new long_writing_file(this));

		std::ostringstream id_oss;
		id_oss << "identifier_"<<__fcount__;
		__fcount__++;
		lf_ptr->_id = id_oss.str();
		lf_ptr->_assumed_filesize = assumed_file_size;
		
#if defined(LinuxPlatform)
		lf_ptr->_path = "/tmp";
#elif defined(Win32Platform)
		lf_ptr->_path = "video";
#endif




		//for debug only: create and return doc
		boost::system::error_code ec;
// 		boost::system::error_code ec = boost::system::make_error_code(boost::future_errc(5));
// 		if (__fcount__ > 2)
// 			ec = boost::system::error_code();
		cb(ec,lf_ptr);


}


long_writing_file::long_writing_file(archive_client* c):a_client(c),_assumed_filesize(0),_current_filesize(0),
		_current_state(s_in_progress){

}

std::string long_writing_file::id() const{
	return _id;
}

boost::filesystem::path long_writing_file::get_path() const{
	return _path;
}

void long_writing_file::set_filename(const std::string& fname){
	_filename = fname;
	//TODO: notify archive server
}
void long_writing_file::set_current_filesize(boost::int64_t fsize){
	_current_filesize = fsize;
	//TODO: notify archive server
}


void long_writing_file::set_state(state s){
	_current_state = s;
	//TODO: notify archive server
}




