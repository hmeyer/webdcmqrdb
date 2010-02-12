#ifndef INDEX_H
#define INDEX_H

#include <string>
#include <boost/scoped_ptr.hpp>
#include <vector>
#include <algorithm>

#include "dcmtk/dcmdata/dctagkey.h"


#include "dcmtk/dcmqrdb/dcmqrdbl.h"
#include <Wt/WAbstractTableModel>

using namespace std;
using namespace boost;
using namespace Wt;

extern const string emptyString;

template< class DType >
class DataList:public WAbstractTableModel {
  public:
    typedef DType DataType;
    typedef std::vector< DataType > DataContainter;
    int columnCount(const WModelIndex &parent = WModelIndex()) const;
    int rowCount(const WModelIndex &parent = WModelIndex()) const;
    boost::any data(const WModelIndex &index, int role = DisplayRole) const;
    boost::any headerData(int section, Orientation orientation = Horizontal, int role = DisplayRole) const;
    DataType &operator[]( int i ) { return myData[i]; }
    void push_back(const DataType &d) { myData.push_back(d); }
    void clear(void) { myData.clear(); }
    const std::string &getUID(int index) const;
  private:
  DataContainter myData;
};

typedef vector< string > StringList;
typedef vector< DcmTagKey > TagList;

template< class ElementData >
class GetFromTag {
  public:
  const string &getFromTag( const DcmTagKey &tag ) const {
    const ElementData *this_ = static_cast< const ElementData* >(this);
    TagList::const_iterator pos = find(ElementData::tags.begin(), ElementData::tags.end(), tag);
    return *(this_->begin() + distance(ElementData::tags.begin(), pos) );
  }
};

class StudyData: public StringList, public GetFromTag< StudyData > {
  public:
    static const TagList tags;
    static const StringList headers;
    StudyData():StringList( tags.size() ) {}
    void t() {
      DcmTagKey x;
      ;
    }
    const string &getUID() const;
};
class SerieData: public StringList {
  public:
    static const TagList tags;
    static const StringList headers;
    SerieData():StringList( tags.size() ) {}
    const string &getUID() const;
};
class ImageData: public StringList {
  public:
    static const TagList tags;
    static const StringList headers;
    ImageData():StringList( tags.size() ) {}
    const string &getUID() const;
};

typedef DataList< StudyData > StudyList;
typedef DataList< SerieData > SerieList;
typedef DataList< ImageData > ImageList;

class Index {
public:
  typedef std::vector< DcmTagKey > TagList;
  Index( const string &path );
  void findStudies( const string &filter, StudyList &studies );
  void getSeries( const vector< string > &studyUIDs, SerieList &series );
  void getImages( const vector< string > &serieUIDs, ImageList &images );
private:
  template< class DataListType >
  void dicomFind( const string &qrlevel, const string &QRModel, const string &param, const DcmTagKey &paramTag, const TagList &tags, DataListType &result );
  scoped_ptr<DcmQueryRetrieveDatabaseHandle> dbHandle;
};

template< class DataType >
int DataList<DataType>::columnCount(const WModelIndex &parent) const {
  return DataType::tags.size();
}
template< class DataType >
int DataList<DataType>::rowCount(const WModelIndex &parent) const {
  return myData.size();
}
template< class DataType >
boost::any DataList<DataType>::data(const WModelIndex &index, int role) const {
  return boost::any( myData[ index.row() ][index.column()] );
}
template< class DataType >
const string &DataList<DataType>::getUID(int index) const {
  if ( index < myData.size() ) return myData[ index ].getUID();
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