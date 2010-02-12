#include "sender.h"
#include <boost/thread/thread.hpp>


#include <iostream>
#include <boost/assign/list_of.hpp>
#include <boost/format.hpp>

using namespace std;
using namespace boost;



void DicomSender::queueJob( SenderLevel l, const string &uid, const string &desc) {
  boost::mutex::scoped_lock lock(joblist_mutex);
  jobs_.push_back( SendJob(l,uid,desc) );
}

void DicomSender::workLoop(void) {
  int c = 0;
  while( !stopWork_ ) {
    cerr << __FUNCTION__ << " (this:" << this << ")" << endl;
    {
      boost::mutex::scoped_lock lock(joblist_mutex);
      cerr << "this is workloop, " << ++c << ". iteration - btw I have " << jobs_.size() << " jobs to do" << endl;
    }
    boost::this_thread::sleep(boost::posix_time::milliseconds(1000)); 
  }
}

DicomSender::DicomSender():stopWork_(false) {
  boost::thread senderThread( boost::bind(&DicomSender::workLoop, this) );
};


DicomSender::JobInfo::JobInfo( const JobListType &joblist, boost::mutex &joblist_mutex )
  :joblist_(joblist), joblist_mutex_(joblist_mutex){ }
int DicomSender::JobInfo::columnCount(const WModelIndex &parent) const {
  return 4;
}
int DicomSender::JobInfo::rowCount(const WModelIndex &parent) const {
  boost::mutex::scoped_lock lock(joblist_mutex_);  // TODO: shared_lock
  return joblist_.size();
}
const vector< string > statusString = assign::list_of("queued")("executing")("finished")("aborted");
boost::any DicomSender::JobInfo::data(const WModelIndex &index, int role) const {
  int c = index.column();
  if (c < 4) {
    boost::mutex::scoped_lock lock(joblist_mutex_); // TODO: shared_lock
    if (index.row() < joblist_.size()) {
      const SendJob &j = joblist_[index.row()];
      switch (c) {
	case 0: return j.description; break;
	case 1: return statusString[j.status] + " " + j.statusString; break;
	case 2: return str( format("%|$2.1f|%") % j.percentFinished ); break;
	case 3: return j.uid; break;
      }
    }
  }
  return string("");
}
const vector< string > JobListHeader = assign::list_of("Description")("status")("Progress")("UID");
boost::any DicomSender::JobInfo::headerData(int section, Orientation orientation, int role) const {
  if (orientation == Horizontal) {
    if (section < JobListHeader.size()) return JobListHeader[ section ];
  }
  return "";
}