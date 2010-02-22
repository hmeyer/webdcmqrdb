#include "sender.h"
#include <boost/thread/thread.hpp>


#include <iostream>
#include <algorithm>
#include <queue>
#include <boost/assign/list_of.hpp>
#include <boost/format.hpp>
#include <boost/filesystem/operations.hpp>

#include <exception>

#include "dicomsender.h"




using namespace std;
namespace fs = boost::filesystem;
using boost::format;
using boost::str;
using boost::ref;



void Sender::queueJob( DicomLevel l, const string &uid, const string &desc, const string &myAE, const DicomConfig::PeerInfoPtr dest, const Index::IndexPtr index) {
  unique_lock jobListLock(joblist_mutex_, get_system_time() + millisec(100));
  if (!jobListLock) throw runtime_error("queueJob: Could not acquire unique joblist-Lock!");
  jobs_[jobIndex_++] = SendJob(l,uid,desc,myAE,dest,index);
}

void Sender::workLoop(void) {
  while( !stopWork_ ) {
    {
      shared_lock jobListLock(joblist_mutex_, get_system_time() + millisec(100));
      if (!jobListLock) throw runtime_error("workLoop: Could not acquire shared joblist-Lock!");
      JobListType::iterator myjobIt =  jobs_.begin();
      bool jobDone = false;
      while( !jobDone && myjobIt != jobs_.end()) {
	SendJob &job = myjobIt->second;
	unique_lock jobLock(*job.job_mutex_, get_system_time() + millisec(100));
	if (!jobLock) throw runtime_error("workLoop: Could not acquire unique job-lock!");
	if (job.status == queued) {
	  job.status = executing;
	  jobLock.unlock();
	  jobListLock.unlock();
	  try {
	    executeJob( job );
	  } catch (std::exception &e) {
	    unique_lock catchJobLock(*job.job_mutex_, get_system_time() + millisec(100));
	    if (!catchJobLock) throw runtime_error("workLoop: Could not acquire unique job-lock!");
	    myjobIt->second.status = aborted;
	    myjobIt->second.statusString = e.what();
	  }
	  jobDone = true;
	}
	myjobIt++;
      }
    }
    boost::this_thread::sleep( millisec(100) ); 
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


Sender::JobTableModel::JobTableModel( const JobListType &joblist, boost::shared_mutex &joblist_mutex )
  :joblist_(joblist), joblist_mutex_(joblist_mutex) { }
const vector< string > JobListHeader = boost::assign::list_of("#")("Description")("Destination")("status")("Progress")("UID");
any Sender::JobTableModel::headerData(int section, Orientation orientation, int role) const {
  if (orientation == Horizontal) {
    if (section < JobListHeader.size()) return JobListHeader[ section ];
  }
  return emptyString;
}
int Sender::JobTableModel::columnCount(const WModelIndex &parent) const {
  return JobListHeader.size();
}
int Sender::JobTableModel::rowCount(const WModelIndex &parent) const {
  shared_lock jobListLock( joblist_mutex_, get_system_time() + millisec(100));
  if (!jobListLock) { return 0; }//throw runtime_error("workLoop: Could not acquire shared joblist-Lock!");
  return joblist_.size();
}
const vector< string > statusString = boost::assign::list_of("queued")("executing")("successful")("aborted");
any Sender::JobTableModel::data(const WModelIndex &index, int role) const {
  int c = index.column();
  if (c < JobListHeader.size()) {
    shared_lock jobListLock(joblist_mutex_, get_system_time() + millisec(20));
    if (!jobListLock) return string("Could acquire shared jobList-Lock!");
    JobListType::const_iterator jobIt = joblist_.begin();
    int row = index.row();
    while(row && jobIt!=joblist_.end()) { row--; jobIt++; }
    if (jobIt != joblist_.end()) {
      const SendJob &j = jobIt->second;
      shared_lock jobLock(*j.job_mutex_, get_system_time() + millisec(20));
      if (!jobLock) return string("Could acquire shared job-Lock!");
      switch (c) {
	case 0: return jobIt->first;
	case 1: return j.description;
	case 2: return j.destination->nickName;
	case 3: return statusString[j.status] + " " + j.statusString;
	case 4: return str( format("%2.1f %%") % j.percentFinished );
	case 5: return j.uid;
      }
    }
  }
  return emptyString;
}

void Sender::updateJobStatus( SendJob &job, SenderStatus status ) {
  unique_lock jobLock(*job.job_mutex_, get_system_time() + millisec(100));
  if (!jobLock) throw runtime_error("workLoop: Could not acquire unique job-lock!");
  job.status = status;
}
void Sender::updateJobStatusString( SendJob &job, const string &status ) {
  unique_lock jobLock(*job.job_mutex_, get_system_time() + millisec(100));
  if (!jobLock) throw runtime_error("workLoop: Could not acquire unique job-lock!");
  job.statusString = status;
}
void Sender::updateJobProgress( SendJob &job, uintmax_t overallSize, uintmax_t overallDone, uintmax_t currentSize, float currentProgress ) {
  float progress;
  if (overallSize != 0) progress = 100.0 * (overallDone + currentSize * currentProgress)  / overallSize;
  else progress = 100.0;
  unique_lock jobLock(*job.job_mutex_, get_system_time() + millisec(100));
  if (!jobLock) throw runtime_error("workLoop: Could not acquire unique job-lock!");
  job.percentFinished = progress;
}


void Sender::executeJob( SendJob &job) {
  Index::MoveJobList myJobList;
  job.index->moveRequest( job.level, job.uid, myJobList );

  queue<uintmax_t> fileSizes; 
  uintmax_t overallSize = 0;
  uintmax_t transferredSize = 0;
  
  Index::MoveJobList::iterator moveJobIt;
  for(moveJobIt = myJobList.begin(); moveJobIt != myJobList.end(); moveJobIt++) {
    uintmax_t size = 0;
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
    uintmax_t currentFileSize; 
    boost::signals::scoped_connection statusConnection = mySender.onStatusUpdate( 
      bind(&Sender::updateJobStatusString, this, job, _1) );
    boost::signals::scoped_connection progressConnection = mySender.onProgressUpdate( 
      bind(&Sender::updateJobProgress, this, job, ref(overallSize), ref(transferredSize), ref(currentFileSize), _1) );
    while (moveJobIt != myJobList.end()) {
      currentFileSize = fileSizes.front(); fileSizes.pop();
      mySender.storeImage( moveJobIt->sopClass, moveJobIt->sopInstance, moveJobIt->imgFile, currentFileSize );
      transferredSize += currentFileSize;
      updateJobProgress(job, overallSize, transferredSize, 0, 0);
      moveJobIt++;
    }
  } catch (std::exception &e) {
    updateJobStatus(job, aborted);
    updateJobStatusString(job, e.what());
    return;
  }
  updateJobProgress(job, overallSize, transferredSize, 0, 0);
  updateJobStatusString(job, "Completed");
  updateJobStatus(job, successful);
}

void Sender::setACSE_Timeout( int timeout ) { acse_timeout_ = timeout;} 
void Sender::setNumStoreRetries( int retries ) { numStoreRetries_ = retries;}
void Sender::setMaxPDU( unsigned int mpdu ) { maxPDU_ = mpdu; }
