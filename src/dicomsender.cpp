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
#include <sstream>
#include <stdexcept>
#include "dicomsender.h"
#include <boost/format.hpp>
#include <boost/scoped_ptr.hpp>

using namespace std;
using boost::scoped_ptr;
using boost::str;
using boost::format;

DicomSender::DicomSender( const string &localAE, DicomConfig::PeerInfoPtr peer, int numStoreRetries, int acse_timeout ): numStoreRetries_(numStoreRetries), 
  assoc_(NULL), net_(NULL), blockMode_(DIMSE_BLOCKING), dimse_timeout_(0), maxReceivePDULength_(16384),
  networkTransferSyntax_(EXS_Unknown), currentNode_( peer ), currentAETitle_( localAE ) {
  OFCondition cond = ASC_initializeNetwork( NET_REQUESTOR, 0, acse_timeout, &net_ );
  if (cond.bad()) throw runtime_error( str( 
    format("Could not initialize Network:%1%") % cond.text() ) );
}

DicomSender::~DicomSender() {
  detachAssociation(true);
  ASC_dropNetwork( &net_ );
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

void DicomSender::addPresentationContexts(T_ASC_Parameters &params) {
    /* abstract syntaxes for storage SOP classes are taken from dcmdata */
    const char *abstractSyntaxes[] = {
      UID_VerificationSOPClass,
      UID_FINDStudyRootQueryRetrieveInformationModel
    };

    /*
    ** We prefer to accept Explicitly encoded transfer syntaxes.
    ** If we are running on a Little Endian machine we prefer
    ** LittleEndianExplicitTransferSyntax to BigEndianTransferSyntax.
    ** Some SCP implementations will just select the first transfer
    ** syntax they support (this is not part of the standard) so
    ** organise the proposed transfer syntaxes to take advantage
    ** of such behaviour.
    */
    unsigned int numTransferSyntaxes = 0;
    const char* transferSyntaxes[] = { NULL, NULL, NULL };

    if (networkTransferSyntax_ == EXS_LittleEndianImplicit)
    {
        transferSyntaxes[0] = UID_LittleEndianImplicitTransferSyntax;
        numTransferSyntaxes = 1;
    }
    else
    {
        /* gLocalByteOrder is defined in dcxfer.h */
        if (gLocalByteOrder == EBO_LittleEndian)
        {
            /* we are on a little endian machine */
            transferSyntaxes[0] = UID_LittleEndianExplicitTransferSyntax;
            transferSyntaxes[1] = UID_BigEndianExplicitTransferSyntax;
            transferSyntaxes[2] = UID_LittleEndianImplicitTransferSyntax;
            numTransferSyntaxes = 3;
        } else {
            /* we are on a big endian machine */
            transferSyntaxes[0] = UID_BigEndianExplicitTransferSyntax;
            transferSyntaxes[1] = UID_LittleEndianExplicitTransferSyntax;
            transferSyntaxes[2] = UID_LittleEndianImplicitTransferSyntax;
            numTransferSyntaxes = 3;
        }
    }

    /* first add presentation contexts for find and verification */
    int pid = 1;
    OFCondition cond = EC_Normal;
    for (int i=0; i<(int)DIM_OF(abstractSyntaxes) && cond.good(); i++)
    {
        cond = ASC_addPresentationContext( &params, pid, abstractSyntaxes[i], transferSyntaxes, numTransferSyntaxes);
        pid += 2; /* only odd presentation context id's */
    }

    /* and then for all storage SOP classes */
    for (int i=0; i<numberOfDcmLongSCUStorageSOPClassUIDs && cond.good(); i++)
    {
      cond = ASC_addPresentationContext( &params, pid, dcmLongSCUStorageSOPClassUIDs[i], transferSyntaxes, numTransferSyntaxes);
      pid += 2;/* only odd presentation context id's */
    }
}
void DicomSender::attachAssociation() {
    if (assoc_ != NULL) detachAssociation(false);

    if (!currentNode_) throw runtime_error("Cannot open association to undefined node");

    T_ASC_Parameters *params;
    OFCondition cond = ASC_createAssociationParameters(&params, maxReceivePDULength_);
    if (cond.bad()) throw runtime_error( str( 
      format("Help, cannot create association parameters:%1%") % cond.text() ) );
    ASC_setAPTitles(params, currentAETitle_.c_str(), currentNode_->AETitle.c_str(), NULL);

    DIC_NODENAME localHost;
    gethostname(localHost, sizeof(localHost) - 1);
    if (currentNode_->hostName.size() == 0 || currentNode_->portNumber== 0) {
      ASC_destroyAssociationParameters(&params);
      throw runtime_error( str(
      format( "Cannot open association to %1%:%2%" ) % currentNode_->hostName % currentNode_->portNumber ));
    }
    string presentationAddress(str(format("%1%:%2%") % currentNode_->hostName % currentNode_->portNumber));
    ASC_setPresentationAddresses(params, localHost, presentationAddress.c_str());

    addPresentationContexts(*params);

    cond = ASC_requestAssociation(net_, params, &assoc_);
    if (cond.bad()) {
        if (cond == DUL_ASSOCIATIONREJECTED) {
            T_ASC_RejectParameters rej;
            ASC_getRejectParameters(params, &rej);
	    ostringstream rejString;
	    ASC_printRejectParameters( rejString, &rej );
            ASC_dropAssociation(assoc_);
            ASC_destroyAssociation(&assoc_);
	    throw runtime_error( str( 
	      format("Association Rejected:%1%") % rejString.str() ) );
        } else {
            ASC_dropAssociation(assoc_);
            ASC_destroyAssociation(&assoc_);
	    throw runtime_error( str( 
	      format("Association Request Failed: Peer (%1%, %2%):%3%") % presentationAddress % currentNode_->AETitle % cond.text() ) );
        }
    }
    if (ASC_countAcceptedPresentationContexts(params) == 0) {
        ASC_abortAssociation(assoc_);
        ASC_dropAssociation(assoc_);
        ASC_destroyAssociation(&assoc_);
	throw runtime_error( str( 
	  format("All Presentation Contexts Refused: Peer (%1%, %2%)") % presentationAddress % currentNode_->AETitle ) );
    }
    updateStatus_( str( format( "New Association Started (%1%,%2%)" ) % presentationAddress % currentNode_->AETitle ) );
}

void DicomSender::changeAssociation() {
    if (assoc_ != NULL) {
        /* do we really need to change the association */
	DIC_AE actualPeerAETitle;
	ASC_getAPTitles(assoc_->params, NULL, actualPeerAETitle, NULL);
	if (!currentNode_) throw runtime_error("Cannot change association to undefined node");
	if (currentNode_->AETitle.compare(actualPeerAETitle) == 0) return;
    }
    detachAssociation(false);
    attachAssociation();
}
void DicomSender::detachAssociation(bool abortFlag) {
  if (assoc_ == NULL) return;  /* nothing to do */

  DIC_NODENAME presentationAddress;
  ASC_getPresentationAddresses(assoc_->params, NULL,
      presentationAddress);
  DIC_AE peerTitle;
  ASC_getAPTitles(assoc_->params, NULL, peerTitle, NULL);

  OFCondition cond = EC_Normal;
  if (abortFlag) {
      /* abort association */
      updateStatus_(str( format( "Aborting Association (%1%)" ) % peerTitle ) );
      cond = ASC_abortAssociation(assoc_);
      if (cond.bad())
	updateStatus_(str( format("Association Abort Failed:%1%") % cond.text() ) );
  } else {
      /* release association */
      updateStatus_(str( format( "Releasing Association (%s)" ) % peerTitle ) );
      cond = ASC_releaseAssociation(assoc_);
      if (cond.bad())
	updateStatus_(str( format("Association Release Failed:%1%") % cond.text() ) );
  }
  ASC_dropAssociation(assoc_);
  ASC_destroyAssociation(&assoc_);

  if (abortFlag) {
      updateStatus_(str( format( "Aborted Association (%1%,%2%)" ) % presentationAddress % peerTitle ) );
  } else {
      updateStatus_(str( format( "Released Association (%1%,%2%)" ) % presentationAddress % peerTitle ) );
  }
}


void DicomSender::storeImage(const string &sopClass, const string &sopInstance, const string &imgFile) {
  typedef scoped_ptr< DcmDataset > DcmDatasetPtr;

  if (sopClass.size() == 0) throw invalid_argument( string("WARNING: deleted image, giving up (no sopClass):") + imgFile );
  ifstream imgStream( imgFile.c_str(), ios_base::binary );
  if (imgStream.fail()) throw runtime_error( string("WARNING: Could not open image, giving up (no imgFile):") + imgFile );

  changeAssociation();
  
  bool sendSuccess = false;
  int numRetries = numStoreRetries_;

  do {
    /* which present	ation context should be used */
    T_ASC_PresentationContextID presId = ASC_findAcceptedPresentationContextID(assoc_, sopClass.c_str());
    if (presId == 0) throw runtime_error( str(
      format("No presentation context for: (%1%): %2%") % dcmSOPClassUIDToModality(sopClass.c_str()) % sopClass ));

    DIC_US msgId = assoc_->nextMsgID++;

    T_DIMSE_C_StoreRQ req;
    initRequest( req, msgId, sopClass, sopInstance, DIMSE_DATASET_PRESENT, DIMSE_PRIORITY_MEDIUM );
    
    DcmFileFormat dcmff;
    OFCondition cond = dcmff.loadFile(imgFile.c_str());
    /* figure out if an error occured while the file was read*/
    if (cond.bad()) throw runtime_error( str(
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
      detachAssociation( true );
      changeAssociation();
    }
  } while(!sendSuccess && (numRetries--)>0);

  imgStream.close();

  return;
}

