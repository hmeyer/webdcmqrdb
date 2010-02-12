#ifndef SENDER_H
#define SENDER_H

#include <string>
#include <vector>
#include <boost/thread/mutex.hpp>

#include <Wt/WAbstractTableModel>

using namespace std;
using namespace Wt;

enum SenderLevel {
  StudyLevel,
  SerieLevel,
  ImageLevel
};

enum SenderStatus {
  queued,
  executing,
  finished,
  aborted
};

struct SendJob {
  SenderLevel level;
  string uid;
  string description;
  SenderStatus status;
  float percentFinished;
  string statusString;
  SendJob( SenderLevel l, string id, string desc ):level(l),uid(id),description(desc),status(queued),percentFinished(0) {};
};

class DicomSender {
  public:
    typedef vector< SendJob > JobListType;
    class JobInfo: public WAbstractTableModel {
      public:
	JobInfo( const JobListType &joblist, boost::mutex &joblist_mutex );
	int columnCount(const WModelIndex &parent = WModelIndex()) const;
	int rowCount(const WModelIndex &parent = WModelIndex()) const;
	boost::any data(const WModelIndex &index, int role = DisplayRole) const;
	boost::any headerData(int section, Orientation orientation = Horizontal, int role = DisplayRole) const;
      protected:
	const JobListType &joblist_;
	boost::mutex &joblist_mutex_;
    };
    DicomSender();
    void queueJob( SenderLevel l, const string &uid, const string &desc);
  protected:
    void workLoop(void);
    bool stopWork_;
    JobListType jobs_;
    boost::mutex joblist_mutex;
};

#endif