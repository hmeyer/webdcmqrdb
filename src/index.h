#ifndef INDEX_H
#define INDEX_H

#include <string>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <map>
#include <vector>
#include <algorithm>
#include <boost/thread/mutex.hpp>

#include "dcmtk/dcmdata/dctagkey.h"
#include "dcmtk/dcmnet/dicom.h"


#include "dcmtk/dcmqrdb/dcmqrdbl.h"
#include <Wt/WAbstractTableModel>

using namespace std;
using boost::any;
using boost::shared_ptr;
using boost::scoped_ptr;

extern const string emptyString;

enum DicomLevel {
  StudyLevel,
  SerieLevel,
  ImageLevel
};

template< class DType >
class DataList:public Wt::WAbstractTableModel, public vector< DType > {
  public:
    typedef DType DataType;
    int columnCount(const Wt::WModelIndex &parent = Wt::WModelIndex()) const;
    int rowCount(const Wt::WModelIndex &parent = Wt::WModelIndex()) const;
    any data(const Wt::WModelIndex &index, int role = Wt::DisplayRole) const;
    any headerData(int section, Wt::Orientation orientation = Wt::Horizontal, int role = Wt::DisplayRole) const;
    const string &getUID(int index) const;
};

typedef map< DcmTagKey, string > TagMap;
typedef vector< string > StringList;
typedef vector< DcmTagKey > TagList;

class ElementData: public TagMap {
  public:
  const string &getFromTag( const DcmTagKey &tag ) const;
  void insertData( const ElementData &other );
  virtual const string &getUID() const = 0;
};

class StudyData: public ElementData {
  public:
    static const TagList tags;
    static const StringList headers;
    const string &getUID() const;
};
class SerieData: public ElementData {
  public:
    static const TagList tags;
    static const StringList headers;
    const string &getUID() const;
};
class ImageData: public ElementData {
  public:
    static const TagList tags;
    static const StringList headers;
    const string &getUID() const;
};

typedef DataList< StudyData > StudyList;
typedef DataList< SerieData > SerieList;
typedef DataList< ImageData > ImageList;


struct MoveJob {
  DIC_UI sopClass;
  DIC_UI sopInstance;
  char imgFile[MAXPATHLEN+1];
};

class Index {
public:
  typedef std::vector< DcmTagKey > TagList;
  typedef std::vector< MoveJob > MoveJobList;
  typedef shared_ptr< Index > IndexPtr;
  Index( const string &path );
  void findStudies( const string &filter, StudyList &studies );
  void getSeries( const vector< StudyData* > &studyUIDs, SerieList &series );
  void getImages( const vector< SerieData* > &serieUIDs, ImageList &images );
  void moveRequest(DicomLevel level, const string &uid, MoveJobList &result);
private:
  template< class DataListType >
  void dicomFind( const string &qrlevel, const string &QRModel, const string &param, const DcmTagKey &paramTag, const TagList &tags, DataListType &result );
  scoped_ptr<DcmQueryRetrieveDatabaseHandle> dbHandle;
  boost::mutex index_mutex_;
};


class IndexDispatcher {
    typedef string StorageAreaType;
    typedef map< StorageAreaType, Index::IndexPtr > IndexMapType;
    IndexMapType index_map_;
    boost::mutex dispatcher_mutex_;
  public:
    Index::IndexPtr getIndexForStorageArea( const StorageAreaType &storage );
};

#include "index.hpp"


#endif