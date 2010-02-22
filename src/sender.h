#ifndef SENDER_H
#define SENDER_H

#include <string>
#include <map>
#include <boost/thread/mutex.hpp>
#include <Wt/WAbstractTableModel>

#include "index.h"
#include "dicomconfig.h"

using namespace std;
using namespace Wt;

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
  SendJob( DicomLevel l, const string &id, const string &desc, const string &myAE, const DicomConfig::PeerInfoPtr dest, Index::IndexPtr idx)
    :level(l),uid(id),description(desc),status(queued),percentFinished(0), myAETitle(myAE), destination(dest), index(idx) {};
  SendJob():level(ImageLevel), status(aborted), percentFinished(0) {};
};

class Sender {
  public:
    typedef map<int, SendJob > JobListType;
    class JobTableModel: public WAbstractTableModel {
      public:
	JobTableModel( const JobListType &joblist, boost::mutex &joblist_mutex );
	virtual int columnCount(const WModelIndex &parent = WModelIndex()) const;
	virtual int rowCount(const WModelIndex &parent = WModelIndex()) const;
	virtual boost::any data(const WModelIndex &index, int role = DisplayRole) const;
	virtual boost::any headerData(int section, Orientation orientation = Horizontal, int role = DisplayRole) const;
      protected:
	const JobListType &joblist_;
	boost::mutex &joblist_mutex_;
      private:
	JobTableModel();
    };
    Sender();
    void queueJob( DicomLevel l, const string &uid, const string &desc, const string &myAE, const DicomConfig::PeerInfoPtr dest, const Index::IndexPtr index);
    JobTableModel &getTableModel(void);
    void updateJobStatus( SendJob &job, const string &status );
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
    boost::mutex joblist_mutex_;
    int numStoreRetries_;
    int acse_timeout_;
    unsigned int maxPDU_;
};

#endif