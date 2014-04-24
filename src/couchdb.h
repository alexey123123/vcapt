#ifndef __couchdb_h__
#define __couchdb_h__


#include <map>
#include <string>

#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>

#include <couchdb/FeedChanges.hpp>
#include <couchdb/Communication2.hpp>
#include <couchdb/Database2.hpp>



namespace couchdb{
	class manager;
	class document{
	public:
		//get,set properties

		template<typename T>
		void set_property(const std::string& prop_name,const T& value, bool if_not_exists){
			boost::unique_lock<boost::shared_mutex> l1(document_internal_mutex);
			if (couch_doc.find(prop_name) != couch_doc.not_found()){

				if (if_not_exists)
					return ;

				T old_value = couch_doc.get<T>(prop_name,T());
				if (old_value==value)
					return ;
				couch_doc.erase(prop_name);
			}
			couch_doc.add<T>(prop_name,value);
			notify_doc_dispatcher();
		};

		template<typename T>
		void set_property(const std::string& prop_name,const T& value){
			return set_property<T>(prop_name,value,false);
		}
		template<typename T>
		void set_property_if_not_exists(const std::string& prop_name,const T& value){
			return set_property<T>(prop_name,value,true);
		}


		template<typename T>
		T get_property(const std::string& prop_name,const T& default_value){
			boost::shared_lock<boost::shared_mutex> l1(document_internal_mutex);
			return couch_doc.get<T>(prop_name,default_value);
		};



		//Tags
		std::string get_tags();
		void set_tags(const std::string& tags);

		bool have_tag(const std::string& tag);
		void add_tag(const std::string& tag);
		void remove_tag(const std::string& tag);



		//TODO: typedef boost::function<void (std::string,std::string,std::string)> ChangeDocumentEvent;//doc_id
		typedef boost::function<void (std::string,std::string,std::string)> change_property_event;//doc_id,property_name,property_value
		typedef boost::function<void ()> delete_document_event;
		struct ticket{
			document* doc;
			std::string ticket_id;

			~ticket();
		};
		typedef boost::shared_ptr<ticket> ticket_ptr;
		ticket_ptr set_change_handler(change_property_event cpe);
		ticket_ptr set_delete_handler(delete_document_event de);
		ticket_ptr set_handlers(change_property_event cpe,delete_document_event de);


		bool empty();
		void clear();

		std::string id() const
		{return document_id;};
	private:
		friend class manager;
		document(manager* m,const std::string& __id, boost::property_tree::ptree pt):document_id(__id),_manager(m),couch_doc(pt){};

		std::string document_id;
		manager* _manager;
		boost::shared_mutex document_internal_mutex;
		boost::property_tree::ptree couch_doc;
		boost::property_tree::wptree couch_doc_w;

		struct change_prop_event{
			change_property_event e;
			std::string ticket_id;
		};
		std::map<std::string,change_prop_event> change_prop_events;
		void delete_change_prop_event(const std::string& ticket_id);


		struct delete_doc_event{
			delete_document_event e;
			std::string ticket_id;
		};
		std::map<std::string,delete_doc_event> delete_doc_events;
		void remove_del_doc_event(const std::string& ticket_id);


		void notify_doc_dispatcher();

	};
	typedef boost::shared_ptr<document> document_ptr;






	class manager: public CouchDB::ChangesFeeder{
	public:
		manager(const std::string& _host,const std::string& _dbname);


		document_ptr get_document(const std::string& doc_id,std::string& error_message);

		document_ptr create_document(const std::string& doc_id,std::string& error_message,std::string tags = "");
		document_ptr create_document(std::string& error_message);


		void delete_document(document_ptr d);

		typedef boost::function<void (std::string,boost::property_tree::ptree)> ChangeHandler;//doc_id,doc
		void set_additional_change_handler(ChangeHandler h);

		bool load_ptree_document(const std::string& id, boost::property_tree::ptree&,		  
			std::string& error_message,
			const std::string &rev="");
		bool save_ptree_document(boost::property_tree::ptree&,std::string& error_message);
		bool delete_ptree_document(boost::property_tree::ptree&,std::string& error_message);

	protected:
		void on_document_change(const std::string& doc_id, boost::property_tree::ptree& doc_ptree);
	private:
		boost::asio::io_service& ios;
		CouchDB::Communication2 comm;
		CouchDB::Database2 db;
		boost::mutex internal_mutex;

		struct internal_doc{
			std::string id;
			std::string rev;

			unsigned int changes_count;
			boost::shared_ptr<boost::asio::steady_timer> save_timer_ptr;

			document_ptr dptr;
			internal_doc():changes_count(0){};
			~internal_doc();
		};

		friend class document;
		void local_document_change(const std::string& doc_id);
		void i_thread_local_document_change(std::string doc_id);

		std::map<std::string,internal_doc> internal_docs;

		void i_thread_save_document(boost::system::error_code ec,std::string doc_id);

		struct additional_change_handler{
			ChangeHandler _handler;
		};
		boost::shared_ptr<additional_change_handler> add_handler_ptr;


		void i_thread_delete_document(document_ptr d);
	};

};



#endif//__couchdb_h__