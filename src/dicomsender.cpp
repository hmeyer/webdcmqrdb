#include "dcmtk/config/osconfig.h"

#define INCLUDE_CSTDLIB
#define INCLUDE_CSTDIO
#define INCLUDE_CSTRING
#define INCLUDE_CSTDARG
#define INCLUDE_CERRNO
#define INCLUDE_CTIME
#define INCLUDE_CSIGNAL
#include "dcmtk/ofstd/ofstdinc.h"
#include "dcmtk/dcmnet/cond.h"
#include "dcmtk/dcmnet/assoc.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/ofstd/ofcmdln.h"
#include "dcmtk/dcmqrdb/dcmqrcnf.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmnet/diutil.h"

#include <iostream>
#include <stdexcept>
#include "dicomsender.h"
#include <boost/format.hpp>
#include <boost/scoped_ptr.hpp>

using namespace std;
using boost::scoped_ptr;
using boost::str;
using boost::format;

DicomSender::DicomSender( int numStoreRetries ): numStoreRetries_(numStoreRetries), 
  assoc_(NULL), blockMode_(DIMSE_BLOCKING), dimse_timeout_(0) {
}

inline void initRequest(T_DIMSE_C_StoreRQ &req, DIC_US msgId, 
		 const string &sopClass, const string &sopInstance, 
		 T_DIMSE_DataSetType dataset, T_DIMSE_Priority priority) {
    bzero((char*)&req, sizeof(req));
    req.MessageID = msgId;
    sopClass.copy( req.AffectedSOPClassUID, sizeof(req.AffectedSOPClassUID)); 
      req.AffectedSOPClassUID[ sizeof(req.AffectedSOPClassUID) ] = '\0';
    sopInstance.copy( req.AffectedSOPInstanceUID, sizeof(req.AffectedSOPInstanceUID)); 
      req.AffectedSOPInstanceUID[ sizeof(req.AffectedSOPInstanceUID) ] = '\0';
    req.DataSetType = dataset;
    req.Priority = priority;
}

boost::signals::connection DicomSender::onStatusUpdate( const StatusUpdateSignalType::slot_type &slot ) {
  return updateStatus_.connect( slot );
}
boost::signals::connection DicomSender::onProgressUpdate( const ProgressUpdateSignalType::slot_type &slot ) {
  return updateProgress_.connect( slot );
}


void DicomSender::storeProgressCallback(void *self,
    T_DIMSE_StoreProgress *progress,
    T_DIMSE_C_StoreRQ * /*req*/) {
    float scalar_progress;
    DicomSender *selfObj = static_cast<DicomSender*>(self);
    switch (progress->state) {
    case DIMSE_StoreBegin:
      scalar_progress = 0.0;
      break;
    case DIMSE_StoreEnd:
      scalar_progress = 1.0;
      break;
    default:
	float totalBytes;
        if (progress->totalBytes == 0)
          totalBytes = selfObj->currentfileSize_;
        else
	  totalBytes = progress->totalBytes;
	scalar_progress = progress->progressBytes / totalBytes;
        break;
    }
    selfObj->updateProgress_( scalar_progress );
}


void DicomSender::storeImage(const string &sopClass, const string &sopInstance, const string &imgFile) {
  cerr << __FUNCTION__ << ": sending:" << imgFile << endl;
  typedef scoped_ptr< DcmDataset > DcmDatasetPtr;

  if (sopClass.size() == 0) throw new invalid_argument( string("WARNING: deleted image, giving up (no sopClass):") + imgFile );
  ifstream imgStream( imgFile.c_str(), ios_base::binary );
  if (imgStream.fail()) throw new runtime_error( string("WARNING: Could not open image, giving up (no imgFile):") + imgFile );

  bool sendSuccess = false;
  int numRetries = numStoreRetries_;
  do {
    /* which present	ation context should be used */
    T_ASC_PresentationContextID presId = ASC_findAcceptedPresentationContextID(assoc_, sopClass.c_str());
    if (presId == 0) throw new runtime_error( str(
      format("No presentation context for: (%1%): %2%") % dcmSOPClassUIDToModality(sopClass.c_str()) % sopClass ));

    DIC_US msgId = assoc_->nextMsgID++;

    T_DIMSE_C_StoreRQ req;
    initRequest( req, msgId, sopClass, sopInstance, DIMSE_DATASET_PRESENT, DIMSE_PRIORITY_MEDIUM );
    
    DcmFileFormat dcmff;
    OFCondition cond = dcmff.loadFile(imgFile.c_str());
    /* figure out if an error occured while the file was read*/
    if (cond.bad()) throw new runtime_error( str(
      format("Bad DICOM file: %1%: %2%") % imgFile % cond.text() ));

    // BEGIN: ON_THE_FLY_COMPRESSION
    T_ASC_PresentationContext pc;
    ASC_findAcceptedPresentationContext(assoc_->params, presId, &pc);
    DcmXfer netTransfer(pc.acceptedTransferSyntax);
    dcmff.getDataset()->chooseRepresentation(netTransfer.getXfer(), NULL);
    // END: ON_THE_FLY_COMPRESSION
    
    DcmDataset *tstd = NULL;
    T_DIMSE_C_StoreRSP rsp;
    currentfileSize_ = DU_fileSize(imgFile.c_str());
    cond = DIMSE_storeUser(assoc_, presId, &req,
	NULL, dcmff.getDataset(), storeProgressCallback, static_cast<void*>(this),
	static_cast<T_DIMSE_BlockingMode>(blockMode_), dimse_timeout_,
	&rsp, &tstd, NULL, currentfileSize_);    

    DcmDatasetPtr stDetail(tstd);
    
    if (cond.good()) {
      sendSuccess = true;
      updateStatus_( str( format( "[MsgID %1%] Complete [Status: %2%]" ) % msgId % DU_cstoreStatusString(rsp.DimseStatus) ) );
    } else {
      sendSuccess = false;
      updateStatus_( str( format( "[MsgID %1%] Failed:%2%" ) % msgId % cond.text() ) );
      cond.text();
      ASC_abortAssociation(assoc_);
      ASC_dropAssociation(assoc_);
      ASC_destroyAssociation(&assoc_);
    }
  } while(!sendSuccess && (numRetries--)>0);

  imgStream.close();

  return;
}

