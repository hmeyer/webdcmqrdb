#ifndef SENDER_H
#define SENDER_H

#include <string>
#include <map>
#include <boost/shared_ptr.hpp>
#include <Wt/WAbstractTableModel>

#include "index.h"
#include "dicomconfig.h"
#include "locks.h"

using namespace std;
using namespace Wt;
using boost::shared_ptr;

enum SenderStatus {
  queued=0,
  executing=1,
  successful=2,
  aborted=3
};

struct SendJob {
  DicomLevel level;
  string uid;
  string description;
  SenderStatus status;
  float percentFinished;
  string statusString;
  string myAETitle;
  DicomConfig::PeerInfoPtr destination;
  Index::IndexPtr index;
  shared_ptr<boost::shared_mutex> job_mutex_;
  SendJob( DicomLevel l, const string &id, const string &desc, const string &myAE, const DicomConfig::PeerInfoPtr dest, Index::IndexPtr idx)
    :level(l),uid(id),description(desc),status(queued),percentFinished(0), myAETitle(myAE), destination(dest), index(idx),job_mutex_(new boost::shared_mutex) {};
  SendJob():level(ImageLevel), status(aborted), percentFinished(0),job_mutex_(new boost::shared_mutex) {};
};

class Sender {
  public:
    typedef map<int, SendJob > JobListType;
    
    class JobTableModel: public WAbstractTableModel {
      public:
	JobTableModel( const JobListType &joblist, boost::shared_mutex &joblist_mutex );
	virtual int columnCount(const WModelIndex &parent = WModelIndex()) const;
	virtual int rowCount(const WModelIndex &parent = WModelIndex()) const;
	virtual boost::any data(const WModelIndex &index, int role = DisplayRole) const;
	virtual boost::any headerData(int section, Orientation orientation = Horizontal, int role = DisplayRole) const;
      protected:
	const JobListType &joblist_;
	boost::shared_mutex &joblist_mutex_;
      private:
	JobTableModel();
    };
    Sender();
    void queueJob( DicomLevel l, const string &uid, const string &desc, const string &myAE, const DicomConfig::PeerInfoPtr dest, const Index::IndexPtr index);
    JobTableModel &getTableModel(void);
    void updateJobStatus( SendJob &job, SenderStatus status );
    void updateJobStatusString( SendJob &job, const string &status );
    void updateJobProgress( SendJob &job, uintmax_t overallSize,  uintmax_t overallDone, uintmax_t currentSize, float currentProgress );
    void setACSE_Timeout( int timeout );
    void setNumStoreRetries( int retries );
    void setMaxPDU( unsigned int mpdu );
  protected:
    void workLoop(void);
    void executeJob( SendJob &job);
    bool stopWork_;
    JobListType jobs_;
    int jobIndex_;
    JobTableModel jobTableModel_;
    shared_mutex joblist_mutex_;
    int numStoreRetries_;
    int acse_timeout_;
    unsigned int maxPDU_;
};

#endif