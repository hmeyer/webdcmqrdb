#include "index.h"

#include "dcmtk/config/osconfig.h"

#define INCLUDE_CSTDLIB
#define INCLUDE_CSTDIO
#define INCLUDE_CSTRING
#define INCLUDE_CSTDARG
#define INCLUDE_CERRNO
#define INCLUDE_CTIME
#define INCLUDE_CSIGNAL
#include "dcmtk/ofstd/ofstdinc.h"
#include "dcmtk/dcmdata/dcdeftag.h"

#include "dcmtk/dcmqrdb/dcmqrdbl.h"
#include "dcmtk/dcmqrdb/dcmqrdbs.h"
#include "dcmtk/dcmnet/diutil.h"
#include <stdexcept>
#include <boost/scoped_ptr.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/foreach.hpp>

#include <iostream>


using namespace std;
using boost::assign::list_of;

typedef scoped_ptr< DcmDataset > DcmDatasetPtr;

const TagList StudyData::tags = list_of(DCM_PatientsName)(DCM_PatientsBirthDate)(DCM_PatientID)
  (DCM_StudyID)(DCM_StudyDate)(DCM_StudyDescription)(DCM_ReferringPhysiciansName)(DCM_InstitutionName)
  (DCM_InstitutionalDepartmentName)(DCM_StudyInstanceUID);

const TagList SerieData::tags = list_of(DCM_SeriesNumber)(DCM_Modality)(DCM_SeriesDescription)
  (DCM_SeriesInstanceUID);

const TagList ImageData::tags = list_of(DCM_InstanceNumber)(DCM_TransferSyntaxUID)(DCM_SOPInstanceUID);
  
  
const StringList StudyData::headers = list_of("Name")("Date of Birth")("PatientID")
  ("StudyID")("Date of Study")("Description")("Referring Physician")("Institution")
  ("Department")("UID");

const StringList SerieData::headers = list_of("Number")("Modality")("Description")("UID");

const StringList ImageData::headers = list_of("Number")("TransferSyntax")("UID");

const string &StudyData::getUID() const { return getFromTag(DCM_StudyInstanceUID); }
const string &SerieData::getUID() const { return getFromTag(DCM_SeriesInstanceUID); }
const string &ImageData::getUID() const { return getFromTag(DCM_SOPInstanceUID); }

Index::Index( const string &path ) {
  OFCondition dbcond = EC_Normal;
  dbHandle.reset( new DcmQueryRetrieveLuceneIndexHandle(
            path.c_str(), 
            DcmQRLuceneReader,
            dbcond) );  
  if (! dbcond.good() ) throw std::runtime_error( string("error opening index at ") + path);
}

template< class DataListType >
void Index::dicomFind( const string &qrlevel, const string &QRModel, const string &param, const DcmTagKey &paramTag, const TagList &tags, DataListType &result ) {
  if (param.size() == 0) throw runtime_error("cannot query index with zero-length query-parameter");
  
    OFCondition dbcond = EC_Normal;
    DcmQueryRetrieveDatabaseStatus dbStatus(STATUS_Pending);
    DcmDatasetPtr query;
    
    query.reset( new DcmDataset );
    if (!query) throw std::runtime_error( string("could not create Query-Object"));

    DU_putStringDOElement(query.get(), DCM_QueryRetrieveLevel, qrlevel.c_str());
    DU_putStringDOElement(query.get(), paramTag, param.c_str());
    for(TagList::const_iterator i = tags.begin(); i!= tags.end(); i++) {
      if (paramTag != *i)
	DU_putStringDOElement(query.get(), *i, NULL);
    }
   
    DcmDatasetPtr reply;
    
    unique_lock indexLock(index_mutex_, millisec(2000));  // TODO: index pool
    if (!indexLock) throw runtime_error("dicomFind: Could not acquire unique index-lock!");
    
    dbcond = dbHandle->startFindRequest(
        QRModel.c_str(), query.get(), &dbStatus);
    if (dbcond.bad()) throw std::runtime_error( "cannot query database:" );

    dbStatus.deleteStatusDetail();
    while(dbStatus.status() == STATUS_Pending) {
	DcmDataset *tr;
        dbcond = dbHandle->nextFindResponse(&tr, &dbStatus);
	reply.reset(tr);
	if (dbcond.bad()) throw std::runtime_error( "database error" );
        if (dbStatus.status() == STATUS_Pending) {
	  OFString t;
	  typename DataListType::DataType item;
	  unsigned int c = 0;
	  for(TagList::const_iterator i = tags.begin(); i!= tags.end(); i++) {
	    reply->findAndGetOFString(*i, t); 
	    item[*i] = t.c_str();
	  }
	  result.push_back( item );
        }
    }
}

void Index::findStudies( const string &filter, StudyList &studies ) {
  studies.clear();
  dicomFind( "STUDY", UID_FINDStudyRootQueryRetrieveInformationModel, filter, DCM_PatientsName, StudyData::tags, studies );
}

void Index::getSeries( const vector< StudyData* > &studies, SerieList &series ) {
  series.clear();
  BOOST_FOREACH( StudyData *study, studies ) {
    dicomFind( "SERIES", UID_FINDStudyRootQueryRetrieveInformationModel, study->getUID(), DCM_StudyInstanceUID, SerieData::tags, series );
    BOOST_FOREACH( SerieData &serie, series ) {
      serie.insertData( *study );
    }
  }
}
void Index::getImages( const vector< SerieData* > &series, ImageList &images ) {
  images.clear();
  BOOST_FOREACH( SerieData *serie, series ) {
    dicomFind( "IMAGE", UID_FINDStudyRootQueryRetrieveInformationModel, serie->getUID(), DCM_SeriesInstanceUID, ImageData::tags, images );
    BOOST_FOREACH( ImageData &image, images ) {
      image.insertData( *serie );
    }
  }
}

const string &ElementData::getFromTag( const DcmTagKey &tag ) const {
  ElementData::const_iterator pos = find(tag);
  if (pos != this->end()) return pos->second;
  return emptyString;
}

void ElementData::insertData( const ElementData &other) {
  BOOST_FOREACH( const ElementData::value_type &pair, other ) {
    this->insert( pair );
  }
}

void Index::moveRequest(DicomLevel level, const string &uid, MoveJobList &result) {
  string QRModel = UID_MOVEStudyRootQueryRetrieveInformationModel;
  string qrlevel; DcmTagKey uidTag;
  DcmQueryRetrieveDatabaseStatus dbStatus(STATUS_Pending);
  
  switch (level) {
    case StudyLevel: qrlevel = "STUDY"; uidTag = DCM_StudyInstanceUID; break;
    case SerieLevel: qrlevel = "SERIES"; uidTag = DCM_SeriesInstanceUID; break;
    case ImageLevel: qrlevel = "IMAGE"; uidTag = DCM_SOPInstanceUID; break;
  }
  DcmDataset query;
  DU_putStringDOElement(&query, DCM_QueryRetrieveLevel, qrlevel.c_str());
  DU_putStringDOElement(&query, uidTag, uid.c_str());
  OFCondition dbcond = EC_Normal;

  unique_lock indexLock(index_mutex_, millisec(2000));  // TODO: index pool
  if (!indexLock) throw runtime_error("moveRequest: Could not acquire unique index-lock!");
  
  dbcond = dbHandle->startMoveRequest(
      QRModel.c_str(), &query, &dbStatus);      
      
  if (dbcond.bad()) throw std::runtime_error( "cannot query database:" );

  MoveJob currentJob;
  DIC_US nRemaining = 0;
  while (dbStatus.status() == STATUS_Pending) {
      dbcond = dbHandle->nextMoveResponse(currentJob.sopClass, currentJob.sopInstance,
	  currentJob.imgFile, &nRemaining, &dbStatus);
      if (dbStatus.status() == STATUS_Pending) result.push_back( currentJob );
      if (dbcond.bad())
	  throw std::runtime_error("database error");
  }
}

Index::IndexPtr IndexDispatcher::getIndexForStorageArea( const StorageAreaType &storage ) {
  unique_lock dispatcherLock(dispatcher_mutex_, millisec(300)); 
  if (!dispatcherLock) throw runtime_error("getIndexForStorageArea: Could not acquire unique dispatcher-lock!");
  
  IndexMapType::iterator currentIdx = index_map_.find( storage );
  if (currentIdx != index_map_.end()) {
    return currentIdx->second;
  } else {
    Index::IndexPtr newIndex( new Index( storage ) );
    index_map_[ storage ] = newIndex;
    return newIndex;
  }
}

