#ifndef __archive_client_h__
#define __archive_client_h__

#include <boost/asio/io_service.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem/path.hpp>

#include "types.h"
#include "couchdb.h"


class long_writing_file;
typedef boost::shared_ptr<long_writing_file> long_writing_file_ptr;

class archive_client{
public:
	archive_client(couchdb::manager& _cdb_manager);
	~archive_client();

	enum file_type{
		ft_video			= 0x01,
		ft_motion_video		= 0x02,
		ft_snapshot			= 0x03,
		ft_motion_snapshot	= 0x04,

		ft_unknown			= 0xff
	};




	//For long-time writings (like video data) archive-server
	//returns couch-db document, which contains filepath, and state fields.
	//client application writing data, updating file state (in document),
	//archive server marks file (in index database) as completed or not



	typedef boost::function<void (boost::system::error_code,long_writing_file_ptr)> create_long_writing_file_callback;
	void create_long_writing_file(
		file_type ft, 
		boost::int64_t assumed_file_size,
		const std::string& author_name,		//camera name or anything else...
		create_long_writing_file_callback cb);


	//For completed files (like a pictures, or short video data) archive server
	//returns only path and id for file. Client application save data and send notification
	//for archive server, that file is ready (or not)

	struct file_details{
		std::string id;
		boost::filesystem::path fpath; //full path (dir + filename)
	};
	typedef boost::function<void (boost::system::error_code,file_details)> create_finished_file_callback;
	//TODO:
	void create_finished_file(
		file_type ft, 
		std::string filename,	//filename only. server construct and return full path
		boost::int64_t completed_file_size,
		const std::string& source_name,
		create_finished_file_callback cb);
	void set_file_state(const std::string& id, file_state fs);
private:
	couchdb::manager& cdb_manager;
	boost::asio::io_service internal_ioservice;
	boost::thread internal_thread;
	void internal_thread_proc();
	boost::shared_ptr<boost::asio::io_service::work> internal_ioservice_work_ptr;


	void i_thread_create_long_writing_file(
		file_type ft, 
		boost::int64_t assumed_file_size,
		const std::string& author_name,		//camera name or anything else...
		create_long_writing_file_callback cb);
};

class long_writing_file{
public:

	std::string id() const;

	boost::filesystem::path get_path() const;

	void set_filename(const std::string& fname);
	void set_current_filesize(boost::int64_t fsize);

	enum state{
		s_completed = 0x00,
		s_in_progress = 0x01
	};
	void set_state(state s);

private:
	friend class archive_client;
	long_writing_file(archive_client* c);

	archive_client* a_client;
	std::string _id;
	boost::filesystem::path _path;
	std::string _filename;

	boost::int64_t _assumed_filesize;
	boost::int64_t _current_filesize;

	state _current_state;

};



#endif//