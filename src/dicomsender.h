#ifndef DICOMSENDER_H
#define DICOMSENDER_H

#include <string>
#include <boost/signal.hpp>

using namespace std;

struct T_ASC_Association;
struct T_DIMSE_StoreProgress;
struct T_DIMSE_C_StoreRQ;



class DicomSender {
  public:
  typedef boost::signal< void (const string&) > StatusUpdateSignalType;
  typedef boost::signal< void (float) > ProgressUpdateSignalType;
  
  DicomSender( int numStoreRetries );
  void storeImage(const string &sopClass, const string &sopInstance, const string &imgFile);
  boost::signals::connection onStatusUpdate( const StatusUpdateSignalType::slot_type &slot );
  boost::signals::connection onProgressUpdate( const ProgressUpdateSignalType::slot_type &slot );
  protected:
  static void storeProgressCallback(void *self,
    T_DIMSE_StoreProgress *progress,
    T_DIMSE_C_StoreRQ * /*req*/);
  int numStoreRetries_;
  int currentfileSize_;
  T_ASC_Association *assoc_;
  int blockMode_;
  int dimse_timeout_;
  StatusUpdateSignalType updateStatus_;
  ProgressUpdateSignalType updateProgress_;
};

#endif // DICOMSENDER_H
