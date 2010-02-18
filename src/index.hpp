#ifndef INDEX_HPP
#define INDEX_HPP


template< class DataType >
int DataList<DataType>::columnCount(const Wt::WModelIndex &parent) const {
  return DataType::tags.size();
}
template< class DataType >
int DataList<DataType>::rowCount(const Wt::WModelIndex &parent) const {
  return this->size();
}
template< class DataType >
any DataList<DataType>::data(const Wt::WModelIndex &index, int role) const {
  DcmTagKey t = DataType::tags[index.column()];
  return (*this)[ index.row() ].getFromTag(t);
}
template< class DataType >
const string &DataList<DataType>::getUID(int index) const {
  if ( index < this->size() ) return (*this)[ index ].getUID();
  else return emptyString;
}
template< class DataType >
any DataList<DataType>::headerData(int section, Wt::Orientation orientation, int role) const {
  if (orientation == Wt::Horizontal && section < DataType::headers.size()) {
    return DataType::headers[section];
  }
  return emptyString;
}


#endif
