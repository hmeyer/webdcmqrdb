#ifndef DICOMCONFIG_H
#define DICOMCONFIG_H

#include <string>
#include <map>
#include <vector>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

using boost::scoped_ptr;
using boost::shared_ptr;
using std::string;
using std::map;
using std::vector;

class DcmQueryRetrieveConfig;

class DicomConfig {
  public:
    struct PeerInfo {
      string nickName;
      string AETitle;
      string hostName;
      int portNumber;
    };
    typedef shared_ptr< PeerInfo > PeerInfoPtr;
    typedef vector< PeerInfoPtr > PeerListType;
    struct DBInfo {
      string storageArea;
      PeerListType peers;
    };
    typedef scoped_ptr< DcmQueryRetrieveConfig > InternalConfigPtr;
    typedef shared_ptr< DBInfo > DBInfoPtr;
    typedef map< string, DBInfoPtr > DBMapType;
  DicomConfig( const string &configFileName );
  ~DicomConfig();
  const DBMapType &getDBMap(void);
  unsigned int getMaxPDU(void) { return maxPDU_; }
  protected:
  InternalConfigPtr config_;
  DBMapType dbMap_;
  unsigned int maxPDU_;
};

#endif // DICOMCONFIG_H
