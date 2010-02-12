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
using namespace boost;

typedef scoped_ptr< DcmDataset > DcmDatasetPtr;

const TagList StudyData::tags = assign::list_of(DCM_PatientsName)(DCM_PatientsBirthDate)(DCM_PatientID)
  (DCM_StudyID)(DCM_StudyDate)(DCM_StudyDescription)(DCM_ReferringPhysiciansName)(DCM_InstitutionName)
  (DCM_InstitutionalDepartmentName)(DCM_StudyInstanceUID);

const TagList SerieData::tags = assign::list_of(DCM_SeriesNumber)(DCM_Modality)(DCM_SeriesDescription)
  (DCM_SeriesInstanceUID);

const TagList ImageData::tags = assign::list_of(DCM_InstanceNumber)(DCM_TransferSyntaxUID)(DCM_SOPInstanceUID);
  
  
const StringList StudyData::headers = assign::list_of("Name")("Date of Birth")("PatientID")
  ("StudyID")("Date")("Description")("Referring Physician")("Institution")
  ("Department")("UID");

const StringList SerieData::headers = assign::list_of("Number")("Modality")("Description")("UID");

const StringList ImageData::headers = assign::list_of("Number")("TransferSyntax")("UID");

const string emptyString;

const string &StudyData::getUID() const { return (*this)[9]; }
const string &SerieData::getUID() const { return (*this)[3]; }
const string &ImageData::getUID() const { return (*this)[2]; }

Index::Index( const string &path ) {
  OFCondition dbcond = EC_Normal;
  dbHandle.reset( new DcmQueryRetrieveLuceneIndexHandle(
            path.c_str(), 
            DcmQRLuceneReader,
            dbcond) );  
  if (! dbcond.good() ) throw new std::runtime_error( string("error opening index at ") + path);
}

template< class DataListType >
void Index::dicomFind( const string &qrlevel, const string &QRModel, const string &param, const DcmTagKey &paramTag, const TagList &tags, DataListType &result ) {
    OFCondition dbcond = EC_Normal;
    DcmQueryRetrieveDatabaseStatus dbStatus(STATUS_Pending);
    DcmDatasetPtr query;
    
    query.reset( new DcmDataset );
    if (!query) throw new std::runtime_error( string("could not create Query-Object"));

    DU_putStringDOElement(query.get(), DCM_QueryRetrieveLevel, qrlevel.c_str());
    DU_putStringDOElement(query.get(), paramTag, param.c_str());
    for(TagList::const_iterator i = tags.begin(); i!= tags.end(); i++) {
      if (paramTag != *i)
	DU_putStringDOElement(query.get(), *i, NULL);
    }
   
    DcmDatasetPtr reply;
    
    dbcond = dbHandle->startFindRequest(
        QRModel.c_str(), query.get(), &dbStatus);
    if (dbcond.bad()) throw new std::runtime_error( string("cannot query database:") );

    dbStatus.deleteStatusDetail();
    while(dbStatus.status() == STATUS_Pending) {
	DcmDataset *tr;
        dbcond = dbHandle->nextFindResponse(&tr, &dbStatus);
	reply.reset(tr);
	if (dbcond.bad()) throw new std::runtime_error( string("database error"));
        if (dbStatus.status() == STATUS_Pending) {
	  OFString t;
	  typename DataListType::DataType item;
	  unsigned int c = 0;
	  for(TagList::const_iterator i = tags.begin(); i!= tags.end(); i++) {
	    reply->findAndGetOFString(*i, t); 
	    item[c++] = t.c_str();
	  }
	  result.push_back( item );
        }
    }
}

void Index::findStudies( const string &filter, StudyList &studies ) {
  studies.clear();
  dicomFind( "STUDY", UID_FINDStudyRootQueryRetrieveInformationModel, filter, DCM_PatientsName, StudyData::tags, studies );
}

void Index::getSeries( const vector< string > &studyUIDs, SerieList &series ) {
  series.clear();
  BOOST_FOREACH( string studyUID, studyUIDs ) {
    dicomFind( "SERIES", UID_FINDStudyRootQueryRetrieveInformationModel, studyUID, DCM_StudyInstanceUID, SerieData::tags, series );
  }
}
void Index::getImages( const vector< string > &serieUIDs, ImageList &images ) {
  images.clear();
  BOOST_FOREACH( string serieUID, serieUIDs ) {
    dicomFind( "IMAGE", UID_FINDStudyRootQueryRetrieveInformationModel, serieUID, DCM_SeriesInstanceUID, ImageData::tags, images );
  }
}
