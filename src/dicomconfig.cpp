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

#include <stdexcept>
#include <boost/format.hpp>
#include <boost/scoped_ptr.hpp>

#include "dicomconfig.h"

#include <iostream>

using boost::format;
using boost::str;
using boost::scoped_ptr;
using namespace std;




void readDBMap( DicomConfig::InternalConfigPtr &config, DicomConfig::DBMapType &dbMap) {
  DcmQueryRetrieveConfigAEEntry * AEList;
  int numAEs;  
  config->getAEList(&AEList, &numAEs);
  for( int i=0; i < numAEs; i++) {
    DcmQueryRetrieveConfigAEEntry &AEEntry( AEList[i] );
    DicomConfig::DBInfoPtr db( new DicomConfig::DBInfo );
    db->storageArea = AEEntry.StorageArea;
    for(int p = 0; p < AEEntry.noOfPeers; p++) {
      DcmQueryRetrieveConfigPeer &cpeer = AEEntry.Peers[p];
      DicomConfig::PeerInfoPtr peer( new DicomConfig::PeerInfo );
      peer->AETitle = cpeer.ApplicationTitle;
      peer->hostName = cpeer.HostName;
      peer->portNumber = cpeer.PortNumber;
      const char *nick = config->symbolicNameForAETitle( cpeer.ApplicationTitle );
      if (nick != NULL) peer->nickName = nick;
      else peer->nickName = peer->AETitle;
      db->peers.push_back( peer );
    }
    dbMap[ AEEntry.ApplicationTitle ] = db;
  }  
}



DicomConfig::DicomConfig( const std::string &configFileName ):config_( new DcmQueryRetrieveConfig ) {
  if (config_->init( configFileName.c_str() )!=1) throw runtime_error( 
    str( format("error reading config file:%1%") % configFileName ) );
  readDBMap( config_, dbMap_ );
  maxPDU_ = config_->getMaxPDUSize();
}

DicomConfig::~DicomConfig() {} // just to make pimpl for DcmQueryRetrieveConfig work

const DicomConfig::DBMapType &DicomConfig::getDBMap(void) {
  return dbMap_;
}
