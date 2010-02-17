#ifndef INDEX_H
#define INDEX_H

#include <string>
#include <boost/scoped_ptr.hpp>
#include <map>
#include <vector>
#include <algorithm>
#include <boost/thread/mutex.hpp>

#include "dcmtk/dcmdata/dctagkey.h"
#include "dcmtk/dcmnet/dicom.h"


#include "dcmtk/dcmqrdb/dcmqrdbl.h"
#include <Wt/WAbstractTableModel>

using namespace std;
using namespace boost;
using namespace Wt;

extern const string emptyString;

enum DicomLevel {
  StudyLevel,
  SerieLevel,
  ImageLevel
};

template< class DType >
class DataList:public WAbstractTableModel, public vector< DType > {
  public:
    typedef DType DataType;
    int columnCount(const WModelIndex &parent = WModelIndex()) const;
    int rowCount(const WModelIndex &parent = WModelIndex()) const;
    boost::any data(const WModelIndex &index, int role = DisplayRole) const;
    boost::any headerData(int section, Orientation orientation = Horizontal, int role = DisplayRole) const;
    const std::string &getUID(int index) const;
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

template< class DataType >
int DataList<DataType>::columnCount(const WModelIndex &parent) const {
  return DataType::tags.size();
}
template< class DataType >
int DataList<DataType>::rowCount(const WModelIndex &parent) const {
  return this->size();
}
template< class DataType >
boost::any DataList<DataType>::data(const WModelIndex &index, int role) const {
  DcmTagKey t = DataType::tags[index.column()];
  return (*this)[ index.row() ].getFromTag(t);
}
template< class DataType >
const string &DataList<DataType>::getUID(int index) const {
  if ( index < this->size() ) return (*this)[ index ].getUID();
  else return emptyString;
}
template< class DataType >
boost::any DataList<DataType>::headerData(int section, Orientation orientation, int role) const {
  if (orientation == Horizontal && section < DataType::headers.size()) {
    return DataType::headers[section];
  }
  return emptyString;
}


#endif