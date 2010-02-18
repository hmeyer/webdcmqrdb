#include "sender.h"
#include <boost/thread/thread.hpp>


#include <iostream>
#include <algorithm>
#include <queue>
#include <boost/assign/list_of.hpp>
#include <boost/format.hpp>
#include <boost/filesystem/operations.hpp>

#include "dicomsender.h"




using namespace std;
namespace fs = boost::filesystem;
using boost::format;
using boost::str;
using boost::ref;



void Sender::queueJob( DicomLevel l, const string &uid, const string &desc, const string &myAE, const DicomConfig::PeerInfoPtr dest, const Index::IndexPtr index) {
  boost::mutex::scoped_lock lock(joblist_mutex_);
  jobs_[jobIndex_++] = SendJob(l,uid,desc,myAE,dest,index);
}

void Sender::workLoop(void) {
  int c = 0;
  while( !stopWork_ ) {
    {
      boost::mutex::scoped_lock lock(joblist_mutex_);
      JobListType::iterator myjobIt;
      for(JobListType::iterator myjobIt = jobs_.begin(); myjobIt != jobs_.end(); myjobIt++)
	if (myjobIt->second.status == queued) {
	  myjobIt->second.status = executing;
	  lock.unlock();
	  executeJob( myjobIt->second );
	  break;
	}
    }
    boost::this_thread::sleep(boost::posix_time::milliseconds(100)); 
  }
}



Sender::Sender():stopWork_(false), jobIndex_(0),
  jobTableModel_(jobs_, joblist_mutex_), 
  numStoreRetries_(5), acse_timeout_(30) {
  boost::thread senderThread( boost::bind(&Sender::workLoop, this) );
};
Sender::JobTableModel &Sender::getTableModel(void) {
  return jobTableModel_;
}


Sender::JobTableModel::JobTableModel( const JobListType &joblist, boost::mutex &joblist_mutex )
  :joblist_(joblist), joblist_mutex_(joblist_mutex) { }
int Sender::JobTableModel::columnCount(const WModelIndex &parent) const {
  return 4;
}
int Sender::JobTableModel::rowCount(const WModelIndex &parent) const {
  boost::mutex::scoped_lock lock(joblist_mutex_);  // TODO: shared_lock
  return joblist_.size();
}
const vector< string > statusString = boost::assign::list_of("queued")("executing")("successful")("aborted");
any Sender::JobTableModel::data(const WModelIndex &index, int role) const {
  int c = index.column();
  if (c < 4) {
    boost::mutex::scoped_lock lock(joblist_mutex_); // TODO: shared_lock
    JobListType::const_iterator jobIt = joblist_.find( index.row() );
    if (jobIt != joblist_.end()) {
      const SendJob &j = jobIt->second;
      switch (c) {
	case 0: return j.description;
	case 1: return statusString[j.status] + " " + j.statusString;
	case 2: return str( format("%2.1f %%") % j.percentFinished );
	case 3: return j.uid;
      }
    }
  }
  return emptyString;
}
const vector< string > JobListHeader = boost::assign::list_of("Description")("status")("Progress")("UID");
any Sender::JobTableModel::headerData(int section, Orientation orientation, int role) const {
  if (orientation == Horizontal) {
    if (section < JobListHeader.size()) return JobListHeader[ section ];
  }
  return emptyString;
}

void Sender::updateJobStatus( SendJob &job, const string &status ) {
  boost::mutex::scoped_lock lock(joblist_mutex_);
  job.statusString = status;
}
void Sender::updateJobProgress( SendJob &job, int overallSize, int overallDone, int currentSize, float currentProgress ) {
  float progress;
  if (overallSize != 0) progress = 100.0 * (overallDone + currentSize * currentProgress)  / overallSize;
  else progress = 100.0;
  boost::mutex::scoped_lock lock(joblist_mutex_);
  job.percentFinished = progress;
}


void Sender::executeJob( SendJob &job) {
  Index::MoveJobList myJobList;
  job.index->moveRequest( job.level, job.uid, myJobList );

  queue<int> fileSizes; int overallSize = 0;
  
  Index::MoveJobList::iterator moveJobIt;
  for(moveJobIt = myJobList.begin(); moveJobIt != myJobList.end(); moveJobIt++) {
    int size = 0;
    fs::path p( moveJobIt->imgFile );
    if (fs::exists( p ) && fs::is_regular( p ))
      size = fs::file_size( moveJobIt->imgFile );
    overallSize += size;
    fileSizes.push( size );
  }
  moveJobIt = myJobList.begin();
  
  DicomSender mySender( job.myAETitle, job.destination, numStoreRetries_, acse_timeout_ );
  if (maxPDU_>0) mySender.setMaxReceivePDULength( maxPDU_ );
  int counter = 0;
  
  
  try {
    int currentFileSize; int transferredSize = 0;
    boost::signals::scoped_connection statusConnection = mySender.onStatusUpdate( 
      bind(&Sender::updateJobStatus, this, job, _1) );
    boost::signals::scoped_connection progressConnection = mySender.onProgressUpdate( 
      bind(&Sender::updateJobProgress, this, job, ref(overallSize), ref(transferredSize), ref(currentFileSize), _1) );
    while (moveJobIt != myJobList.end()) {
      currentFileSize = fileSizes.front(); fileSizes.pop();
      mySender.storeImage( moveJobIt->sopClass, moveJobIt->sopInstance, moveJobIt->imgFile );
      transferredSize += currentFileSize;
      {
	boost::mutex::scoped_lock lock(joblist_mutex_);
	if (overallSize != 0) job.percentFinished = 100.0 * float(transferredSize) / overallSize;
      }
      moveJobIt++;
    }
  } catch (std::exception *e) {
    boost::mutex::scoped_lock lock(joblist_mutex_);
    job.status = aborted;
    job.statusString = e->what();
    return;
  }
  boost::mutex::scoped_lock lock(joblist_mutex_);
  job.percentFinished = 100.0;
  job.status = successful;
  job.statusString = "Completed";
}

void Sender::setACSE_Timeout( int timeout ) { acse_timeout_ = timeout;} 
void Sender::setNumStoreRetries( int retries ) { numStoreRetries_ = retries;}
void Sender::setMaxPDU( unsigned int mpdu ) { maxPDU_ = mpdu; }
