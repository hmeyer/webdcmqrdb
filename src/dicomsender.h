#ifndef DICOMSENDER_H
#define DICOMSENDER_H

#include <string>
#include <boost/signal.hpp>
#include <stdint.h>

#include "dicomconfig.h"

using namespace std;

struct T_ASC_Association;
struct T_DIMSE_StoreProgress;
struct T_DIMSE_C_StoreRQ;
struct T_ASC_Parameters;
struct T_ASC_Network;

struct DicomNode {
  string peerTitle;
  string presentationAddress;
  string peer;
  int port;
  DicomNode():port(0) {}
};



class DicomSender {
  public:
  typedef boost::signal< void (const string&) > StatusUpdateSignalType;
  typedef boost::signal< void (float) > ProgressUpdateSignalType;
  
  DicomSender( const string &localAE, DicomConfig::PeerInfoPtr peer, int numStoreRetries, int acse_timeout );
  ~DicomSender();
  void storeImage(const string &sopClass, const string &sopInstance, const string &imgFile, uintmax_t filesize);
  boost::signals::connection onStatusUpdate( const StatusUpdateSignalType::slot_type &slot );
  boost::signals::connection onProgressUpdate( const ProgressUpdateSignalType::slot_type &slot );
  void setMaxReceivePDULength( int maxPDU ) { maxReceivePDULength_ = maxPDU; }
  protected:
  static void storeProgressCallback(void *self,
    T_DIMSE_StoreProgress *progress,
    T_DIMSE_C_StoreRQ * /*req*/);
    
  void attachAssociation();
  void changeAssociation();
  void detachAssociation(bool abortFlag = false);
  void addPresentationContexts(T_ASC_Parameters &params);
  int numStoreRetries_;
  int currentfileSize_;
  T_ASC_Association *assoc_;
  T_ASC_Network *net_;
  int blockMode_;
  int dimse_timeout_;
  StatusUpdateSignalType updateStatus_;
  ProgressUpdateSignalType updateProgress_;
  DicomConfig::PeerInfoPtr currentNode_;
  string currentAETitle_;
  unsigned int maxReceivePDULength_;
  int networkTransferSyntax_;
};

#endif // DICOMSENDER_H
