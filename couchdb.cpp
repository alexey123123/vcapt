

#include <deque>

#include <boost/foreach.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <utility/Pack.h>

#include "couchdb.h"

boost::uuids::basic_random_generator<boost::mt19937> gen;

std::string generate_ticket_id(){
	boost::uuids::uuid final_uid = gen();
	return boost::uuids::to_string(final_uid);

}



const std::string TagsPropertyName = "device_tags";




using namespace couchdb;


manager::manager(const std::string& _host,const std::string& _dbname):
									CouchDB::ChangesFeeder(_host,_dbname,false),
									ios(get_ioservice()),
									comm(_host),
									db(comm,_dbname),
									add_handler_ptr(){

									};


typedef std::map<std::string,std::string> PropsMap;
PropsMap ptree_to_props_map(boost::property_tree::ptree& pt){
	std::map<std::string,std::string> ret_map;
	BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, pt)
	{
		std::string p_name = v.first;
		std::string p_value = pt.get<std::string>(v.first,"");

		if (p_name=="_id")
			continue;
		if (p_name=="_rev")
			continue;

		ret_map[p_name] = p_value;

	}
	return ret_map;

}


void manager::on_document_change(const std::string& doc_id, boost::property_tree::ptree& doc_ptree){

	boost::unique_lock<boost::mutex> l1(internal_mutex);

	


	if (internal_docs.find(doc_id)==internal_docs.end())
		return ;

	if (doc_ptree != boost::property_tree::ptree()){
		//document changed
		std::string new_rev = doc_ptree.get<std::string>("_rev","");
		if (new_rev=="")
			return ;
		if (new_rev == internal_docs[doc_id].rev)
			return ;

		doc_ptree.erase("_rev");
		doc_ptree.erase("_id");

		//revision updated
		document_ptr dptr = internal_docs[doc_id].dptr;
		internal_docs[doc_id].rev = new_rev;
		dptr->document_internal_mutex.lock();


		if (dptr->change_prop_events.size()){
			boost::property_tree::ptree old_ptree = dptr->couch_doc;


			PropsMap current_map = ptree_to_props_map(old_ptree);
			PropsMap new_map = ptree_to_props_map(doc_ptree);

			PropsMap updated_props,deleted_props;


			BOOST_FOREACH(PropsMap::value_type& vt,current_map){
				if (new_map.find(vt.first)==new_map.end()){
					deleted_props[vt.first] = vt.second;
				} else
					if (new_map[vt.first] != vt.second)
						updated_props[vt.first] = new_map[vt.first];
			}
			BOOST_FOREACH(PropsMap::value_type& vt,new_map){
				if (current_map.find(vt.first)==current_map.end()){
					updated_props[vt.first] = vt.second;
				}
			}

			typedef std::map<std::string,document::change_prop_event>::value_type VT1;
			BOOST_FOREACH(VT1& vt1,dptr->change_prop_events){
				BOOST_FOREACH(PropsMap::value_type& vt,updated_props){
					vt1.second.e(doc_id,vt.first,vt.second);
				}

			}
			//TODO: deleted props events

		}

		dptr->couch_doc = doc_ptree;
		dptr->document_internal_mutex.unlock();

	} else{
		//document deleted
		internal_doc id = internal_docs[doc_id];
		document_ptr dptr1 = id.dptr;
		dptr1->document_internal_mutex.lock();
		typedef std::map<std::string,document::delete_doc_event>::value_type VT1;
		BOOST_FOREACH(VT1& vt1,dptr1->delete_doc_events){
			vt1.second.e();
		}
		dptr1->document_internal_mutex.unlock();
		internal_docs.erase(doc_id);



	}



}


void document::notify_doc_dispatcher(){
	_manager->local_document_change(document_id);
}


void manager::local_document_change(const std::string& doc_id){
	ios.post(boost::bind(&manager::i_thread_local_document_change,this,doc_id));
}
void manager::i_thread_local_document_change(std::string doc_id){
	boost::unique_lock<boost::mutex> l1(internal_mutex);
	if (internal_docs.find(doc_id)==internal_docs.end())
		return ;

	

	internal_docs[doc_id].changes_count++;
	boost::system::error_code ec;
	internal_docs[doc_id].save_timer_ptr->cancel(ec);
	internal_docs[doc_id].save_timer_ptr->expires_from_now(boost::chrono::milliseconds(25));
	internal_docs[doc_id].save_timer_ptr->async_wait(boost::bind(&manager::i_thread_save_document,this,
		boost::asio::placeholders::error,doc_id));

}

void manager::i_thread_save_document(boost::system::error_code ec,std::string doc_id){
	if (ec)
		return ;




	boost::unique_lock<boost::mutex> l1(internal_mutex);
	if (internal_docs.find(doc_id)==internal_docs.end())
		return ;
	//printf("1\n");

	document_ptr dptr = internal_docs[doc_id].dptr;
	dptr->document_internal_mutex.lock();
	boost::property_tree::ptree props_ptree = dptr->couch_doc;
	int changes_count = internal_docs[doc_id].changes_count;
	internal_docs[doc_id].changes_count = 0;
	dptr->document_internal_mutex.unlock();
	//printf("2\n");

	props_ptree.add<std::string>("_id",internal_docs[doc_id].id);
	props_ptree.add<std::string>("_rev",internal_docs[doc_id].rev);
	std::string error_message;
	
	//printf("3\n");


	bool save_result = db.SavePTreeDocument(props_ptree,error_message);
	if (!save_result){
		//save property error
		// - database connection fail
		// - old revision (database have newest version of doc. need wait)
		internal_docs[doc_id].changes_count = changes_count;

		internal_docs[doc_id].save_timer_ptr->expires_from_now(boost::chrono::seconds(1));
		internal_docs[doc_id].save_timer_ptr->async_wait(boost::bind(&manager::i_thread_save_document,this,
			boost::asio::placeholders::error,doc_id));

	} else{
		internal_docs[doc_id].rev = props_ptree.get<std::string>("_rev","");
	}

	//printf("saved\n");
}


manager::internal_doc::~internal_doc(){
}

document_ptr manager::get_document(const std::string& doc_id,std::string& error_message){
	boost::unique_lock<boost::mutex> l1(internal_mutex);

	document_ptr dptr = document_ptr();

	if (internal_docs.find(doc_id)!=internal_docs.end())
		return internal_docs[doc_id].dptr;


	boost::property_tree::ptree p1;

	if (!db.LoadPTreeDocument(doc_id,p1,error_message))
		return document_ptr();

	



	internal_doc d;
	d.id = doc_id;
	d.rev = p1.get<std::string>("_rev","");
	p1.erase("_rev");
	p1.erase("_id");


	d.dptr = document_ptr(new document(this,doc_id,p1));

	d.save_timer_ptr = boost::shared_ptr<boost::asio::steady_timer>(
		new boost::asio::steady_timer(get_ioservice()));


	internal_docs[doc_id] = d;

	return d.dptr;
}


document_ptr manager::create_document(const std::string& doc_id,std::string& error_message,std::string tags){
	boost::unique_lock<boost::mutex> l1(internal_mutex);


	boost::property_tree::ptree pt;
	if (doc_id != "")
		pt.add<std::string>("_id",doc_id);

	pt.add<std::string>(TagsPropertyName,tags);

	if (!db.SavePTreeDocument(pt,error_message))
		return document_ptr();

	std::string new_doc_id = pt.get<std::string>("_id");

	l1.unlock();
	return get_document(new_doc_id,error_message);
}
document_ptr manager::create_document(std::string& error_message){
	return create_document("",error_message);
}

document::ticket::~ticket(){
	doc->delete_change_prop_event(ticket_id);
}

void document::delete_change_prop_event(const std::string& ticket_id){
	boost::unique_lock<boost::shared_mutex> l1(document_internal_mutex);
	change_prop_events.erase(ticket_id);
	delete_doc_events.erase(ticket_id);
}
document::ticket_ptr document::set_change_handler(change_property_event cpe){
	
	ticket_ptr tptr(new ticket());
	tptr->doc = this;
	tptr->ticket_id = generate_ticket_id();

	boost::unique_lock<boost::shared_mutex> l1(document_internal_mutex);
	change_prop_event ed;
	ed.e = cpe;
	ed.ticket_id = tptr->ticket_id;
	change_prop_events[ed.ticket_id] = ed;
	
	
	return tptr;
}
document::ticket_ptr document::set_delete_handler(delete_document_event de){
	ticket_ptr tptr(new ticket());
	tptr->doc = this;
	tptr->ticket_id = generate_ticket_id();

	boost::unique_lock<boost::shared_mutex> l1(document_internal_mutex);
	delete_doc_event dde;
	dde.e = de;
	dde.ticket_id = tptr->ticket_id;
	delete_doc_events[dde.ticket_id] = dde;


	return tptr;
}
document::ticket_ptr document::set_handlers(change_property_event cpe,delete_document_event de){

	ticket_ptr tptr(new ticket());
	tptr->doc = this;
	tptr->ticket_id = generate_ticket_id();

	boost::unique_lock<boost::shared_mutex> l1(document_internal_mutex);

	change_prop_event ed;
	ed.e = cpe;
	ed.ticket_id = tptr->ticket_id;
	change_prop_events[ed.ticket_id] = ed;

	delete_doc_event dde;
	dde.e = de;
	dde.ticket_id = tptr->ticket_id;
	delete_doc_events[dde.ticket_id] = dde;

	return tptr;
}




bool document::empty(){
	boost::shared_lock<boost::shared_mutex> l1(document_internal_mutex);
	return couch_doc.size()==0;
};
void document::clear(){
	boost::unique_lock<boost::shared_mutex> l1(document_internal_mutex);
	couch_doc.clear();
	notify_doc_dispatcher();
}



std::string document::get_tags(){
	return get_property<std::string>(TagsPropertyName,"");
}

void document::set_tags(const std::string& tags){
	set_property(TagsPropertyName,tags);
}

bool document::have_tag(const std::string& tag){
	std::string tags = get_property<std::string>(TagsPropertyName,"");
	std::deque<std::string> tags_deque = Utility::Pack::__parse_string_by_separator__(tags,",");
	if (std::find(tags_deque.begin(),tags_deque.end(),tag) != tags_deque.end())
		return true;

	return false;
}
void document::add_tag(const std::string& tag){
	if (have_tag(tag))
		return;
	std::string tags = get_property<std::string>(TagsPropertyName,"");
	if (tags.size()!=0)
		tags += ",";
	tags += tag;
	set_property<std::string>(TagsPropertyName,tags);
}
void document::remove_tag(const std::string& tag){
	std::string tags = get_property<std::string>(TagsPropertyName,"");
	std::deque<std::string> tags_deque = Utility::Pack::__parse_string_by_separator__(tags,",");
	std::deque<std::string> new_tags_deque;
	bool wch = false;
	std::string t;

	BOOST_FOREACH(t,tags_deque){
		if (t!=tag)
			new_tags_deque.push_back(t); else
			wch = true;
	}

	if (wch){
		tags = "";
		if (new_tags_deque.size()>0)
			BOOST_FOREACH(t,new_tags_deque){
				if (tags.size() != 0)
					tags += ",";
				tags += t;
		}

		set_property(TagsPropertyName,tags);
	}

}



void manager::set_additional_change_handler(ChangeHandler h){
	if (!add_handler_ptr)
		add_handler_ptr = boost::shared_ptr<additional_change_handler>(new additional_change_handler());
	add_handler_ptr->_handler = h;
}


void manager::delete_document(document_ptr d){
	ios.post(boost::bind(&manager::i_thread_delete_document,this,d));
}
void manager::i_thread_delete_document(document_ptr d){
	boost::unique_lock<boost::mutex> l1(internal_mutex);

	if (internal_docs.find(d->id())==internal_docs.end())
		return ;

	std::string rev = internal_docs[d->id()].rev;

	std::string error_message;
	if (!db.DeleteDocument(d->id(),rev,error_message))
		std::cerr<<"DocumentManager: cannot delete document:"<<error_message<<std::endl;

	internal_docs.erase(d->id());
}

bool manager::load_ptree_document(const std::string& id, boost::property_tree::ptree& pt,		  
	std::string& error_message,
	const std::string &rev){
		return db.LoadPTreeDocument(id,pt,error_message,rev);

}

bool manager::save_ptree_document(boost::property_tree::ptree& pt,std::string& error_message){
	return db.SavePTreeDocument(pt,error_message);
}


bool manager::delete_ptree_document(boost::property_tree::ptree& pt,std::string& error_message){
	return db.DeleteDocument(
		pt.get<std::string>("_id"),
		pt.get<std::string>("_rev"),
		error_message);
}



