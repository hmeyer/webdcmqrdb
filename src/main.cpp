/*
 * Copyright (C) 2008 Emweb bvba, Heverlee, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include <Wt/WApplication>
#include <Wt/WContainerWidget>
#include <Wt/WTabWidget>
#include <Wt/WVBoxLayout>
#include <Wt/WText>


#include <Wt/Ext/LineEdit>
#include <Wt/Ext/Button>
#include <Wt/Ext/TableView>
#include <Wt/Ext/ToolBar>
#include <Wt/Ext/ComboBox>

#include <boost/lexical_cast.hpp>


#include "index.h"
#include "sender.h"
#include "dicomconfig.h"

#include <algorithm>
#include <string>
#include <boost/range/end.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

#include <cstdlib>

#include "dcmtk/dcmdata/dcdeftag.h"


using namespace Wt;
using namespace std;
using namespace boost;

void wstring2string( const wstring &w, string &s) {
  s.assign( w.begin(), w.end() );
}

typedef shared_ptr< IndexDispatcher > IndexDispatcherPtr;
typedef shared_ptr< Sender > SenderPtr;
typedef shared_ptr< DicomConfig > ConfigPtr;

const string emptyString;
IndexDispatcherPtr myIndexDispatcher;
SenderPtr mySender;
ConfigPtr myConfig;

/*
 * A simple hello world application class which demonstrates how to react
 * to events, read input, and give feed-back.
 */
class DcmQRDBApplication : public WApplication
{
public:
  DcmQRDBApplication(const WEnvironment& env, ConfigPtr config, IndexDispatcherPtr indexDispatcher, SenderPtr sender );

protected:

private:
  Ext::LineEdit *searchEdit_;
  Ext::Button *sendStudiesButton_;
  Ext::Button *sendSeriesButton_;
  Ext::Button *sendImagesButton_;
  Ext::TableView *studyTable_;
  Ext::TableView *serieTable_;
  Ext::TableView *imageTable_;
  Ext::TableView *jobTable_;
  Ext::ComboBox *dataBaseBox_;
  Ext::ComboBox *dicomNodeBox_;
  boost::signals::scoped_connection onUpdateJobConnection_;
  ConfigPtr config_;
  IndexDispatcherPtr indexDispatcher_;
  Index::IndexPtr index_;
  SenderPtr sender_;
  StudyList studies_;
  SerieList series_;
  ImageList images_;
  string localAETitle_;
  WText *searchStatus_;
  DicomConfig::PeerInfoPtr selectedDestinationPeer_;
  DicomConfig::DBInfoPtr selectedDBInfo_;
  void searchIndex( bool background = false );
  void dataBaseSelectionChanged(void);
  void dicomNodeSelectionChanged(void);
  void studySelectionChanged(void);
  void serieSelectionChanged(void);
  void imageSelectionChanged(void);
  void sendStudies(void);
  void sendSeries(void);
  void sendImages(void);
};

/*
 * The env argument contains information about the new session, and
 * the initial request. It must be passed to the WApplication
 * constructor so it is typically also an argument for your custom
 * application constructor.
*/
DcmQRDBApplication::DcmQRDBApplication(const WEnvironment& env, ConfigPtr config, IndexDispatcherPtr indexDispatcher, SenderPtr sender)
  : WApplication(env), config_(config), indexDispatcher_(indexDispatcher), sender_(sender)
{
  setTitle("Dicom-Web-DataBase");                               // application title
  
  WTabWidget *tabs = new WTabWidget;
  WContainerWidget *searchContainer = new WContainerWidget;
  WContainerWidget *jobContainer = new WContainerWidget;
  WContainerWidget *configContainer = new WContainerWidget;
  tabs->addTab( searchContainer, "Search Patients" );
  tabs->addTab( jobContainer, "Jobs" );
  tabs->addTab( configContainer, "Config" );
//  tabs->addTab( jobsPanel );
//  tabs->addTab( configPanel );
  
  root()->addWidget( tabs );
//  root()->resize( WLength(100, WLength::FontEm) , WLength(100, WLength::FontEm) );
  tabs->resize( WLength::Auto , WLength(100, WLength::Percentage) );
//  searchContainer->resize( WLength(100, WLength::FontEm) , WLength(100, WLength::FontEm) );

  searchEdit_ = new Ext::LineEdit;                     // allow text input
  searchEdit_->setFocus();                                 // give focus
  
  searchContainer->addWidget( searchEdit_ );

  Ext::Button *b = new Ext::Button("Search"); // create a button
  b->setMargin(5, Left);                                 // add 5 pixels margin
  searchContainer->addWidget( b );

  searchContainer->addWidget(new WBreak );                       // insert a line break

  studyTable_ = new Ext::TableView;
  studyTable_->setModel( &studies_ );
  studyTable_->setDataLocation(Ext::ServerSide);
  studyTable_->setSelectionBehavior(SelectRows);
  studyTable_->setSelectionMode(ExtendedSelection);
  studyTable_->itemSelectionChanged().connect( boost::bind(&DcmQRDBApplication::studySelectionChanged, this) );
  studyTable_->setPageSize(50);
  studyTable_->setBottomToolBar(studyTable_->createPagingToolBar());
  sendStudiesButton_ = new Ext::Button("Send");
  sendStudiesButton_->disable();
  sendStudiesButton_->clicked().connect( boost::bind(&DcmQRDBApplication::sendStudies, this) );
  studyTable_->bottomToolBar()->add( sendStudiesButton_ );
  searchStatus_ = new WText;
  studyTable_->bottomToolBar()->addStretch();
  studyTable_->bottomToolBar()->add( searchStatus_ );
  studyTable_->resize( WLength::Auto, WLength(16, WLength::FontEm) );
  searchContainer->addWidget( studyTable_ );

  serieTable_ = new Ext::TableView;
  serieTable_->setModel( &series_ );
  serieTable_->setDataLocation(Ext::ServerSide);
  serieTable_->setSelectionBehavior(SelectRows);
  serieTable_->setSelectionMode(ExtendedSelection);
  serieTable_->itemSelectionChanged().connect( boost::bind(&DcmQRDBApplication::serieSelectionChanged, this) );
  serieTable_->setPageSize(50);
  serieTable_->setBottomToolBar(serieTable_->createPagingToolBar());
  sendSeriesButton_ = new Ext::Button("Send");
  sendSeriesButton_->disable();
  sendSeriesButton_->clicked().connect( boost::bind(&DcmQRDBApplication::sendSeries, this) );
  serieTable_->bottomToolBar()->add( sendSeriesButton_ );
  serieTable_->resize( WLength::Auto, WLength(9, WLength::FontEm) );
  searchContainer->addWidget( serieTable_ );

  imageTable_ = new Ext::TableView;
  imageTable_->setModel( &images_ );
  imageTable_->setDataLocation(Ext::ServerSide);
  imageTable_->setSelectionBehavior(SelectRows);
  imageTable_->setSelectionMode(ExtendedSelection);
  imageTable_->itemSelectionChanged().connect( boost::bind(&DcmQRDBApplication::imageSelectionChanged, this) );
  imageTable_->setPageSize(50);
  imageTable_->setBottomToolBar(imageTable_->createPagingToolBar());
  sendImagesButton_ = new Ext::Button("Send");
  sendImagesButton_->disable();
  sendImagesButton_->clicked().connect( boost::bind(&DcmQRDBApplication::sendImages, this) );
  imageTable_->bottomToolBar()->add( sendImagesButton_ );
  imageTable_->resize( WLength::Auto, WLength(9, WLength::FontEm) );
  searchContainer->addWidget( imageTable_ );  
  /*
   * Connect signals with slots
   *
   * - simple Wt-way
   */
//  b->clicked().connect(this, &DcmQRDBApplication::searchIndex);
  b->clicked().connect
    (boost::bind(&DcmQRDBApplication::searchIndex, this, false));

  /*
   * - using an arbitrary function object (binding values with boost::bind())
   */
  searchEdit_->keyWentUp().connect
    (boost::bind(&DcmQRDBApplication::searchIndex, this, true));
    
    
  jobTable_ = new Ext::TableView;
  jobTable_->setModel( &sender_->getTableModel() );
  jobTable_->setDataLocation(Ext::ServerSide);
  jobTable_->setSelectionBehavior(SelectRows);
  jobTable_->setSelectionMode(ExtendedSelection);
  jobTable_->setPageSize(20);
  jobTable_->resize( WLength::Auto, WLength(30, WLength::FontEm) );
  jobTable_->setBottomToolBar(jobTable_->createPagingToolBar());
  jobContainer->addWidget( jobTable_ );
  
  configContainer->addWidget( new WText("Local DataBase to use:"));
  dataBaseBox_ = new Ext::ComboBox;
  dataBaseBox_->setEditable( false );
  dataBaseBox_->activated().connect( boost::bind(&DcmQRDBApplication::dataBaseSelectionChanged, this) );
  const DicomConfig::DBMapType &dbMap( config_->getDBMap() );
  for( DicomConfig::DBMapType::const_iterator i = dbMap.begin(); i != dbMap.end(); i++)
    dataBaseBox_->addItem( i->first );
  configContainer->addWidget( dataBaseBox_ );
  configContainer->addWidget(new WBreak );                       // insert a line break
  configContainer->addWidget( new WText("Destination Dicom Node:"));
  dicomNodeBox_ = new Ext::ComboBox;
  dicomNodeBox_->activated().connect( boost::bind(&DcmQRDBApplication::dicomNodeSelectionChanged, this) );
  configContainer->addWidget( dicomNodeBox_ );
  dataBaseBox_->setCurrentIndex(0);dataBaseSelectionChanged();
}

void DcmQRDBApplication::dataBaseSelectionChanged(void) {
  studies_.clear();
  studyTable_->setModel( &studies_ );
  int idx = dataBaseBox_->currentIndex();
  int c = 0;
  const DicomConfig::DBMapType &dbMap( config_->getDBMap() );
  DicomConfig::DBMapType::const_iterator it = dbMap.begin(); 
  while( c!=idx && it != dbMap.end() ) {
    c++;
    it++;
  }
  if (it==dbMap.end()) return;
  selectedDBInfo_ = it->second;
  try {
    index_ = indexDispatcher_->getIndexForStorageArea( selectedDBInfo_->storageArea );
  } catch (std::exception &e) {
    searchStatus_->setText( e.what() );
  }
  localAETitle_ = it->first;
  dicomNodeBox_->clear();
  for(DicomConfig::PeerListType::const_iterator i = selectedDBInfo_->peers.begin(); i != selectedDBInfo_->peers.end(); i++) {
    const DicomConfig::PeerInfoPtr peer = *i;
    dicomNodeBox_->addItem( peer->nickName );
  }
  dicomNodeBox_->setCurrentIndex(0);
  dicomNodeSelectionChanged();
}

void DcmQRDBApplication::dicomNodeSelectionChanged(void) {
  int idx = dicomNodeBox_->currentIndex();
  selectedDestinationPeer_ = selectedDBInfo_->peers[idx];
}


void DcmQRDBApplication::studySelectionChanged(void) {
  const std::vector< int > &rows = studyTable_->selectedRows();
  if (rows.size() > 0) {
    sendStudiesButton_->enable();
    vector< StudyData* > stlist;
    for(vector< int >::const_iterator i = rows.begin(); i != rows.end(); i++)
      stlist.push_back( &studies_[*i] );
    try {
      index_->getSeries( stlist, series_ );
    } catch (std::exception &e) {
      searchStatus_->setText( e.what() );
    }
    serieTable_->setModel( &series_ );
  }
  else {
    sendStudiesButton_->disable();
    series_.clear();
    images_.clear();
  }
}
void DcmQRDBApplication::serieSelectionChanged(void) {
  const std::vector< int > &rows = serieTable_->selectedRows();
  if (rows.size() > 0) {
    sendSeriesButton_->enable();
    vector< SerieData* > selist;
    for(vector< int >::const_iterator i = rows.begin(); i != rows.end(); i++) {
      string uid;
      uid = series_.getUID( *i );
      selist.push_back( &series_[*i] );
    }
    try {
      index_->getImages( selist, images_ );
    } catch (std::exception &e) {
      searchStatus_->setText( e.what() );
    }
    imageTable_->setModel( &images_ );
  }
  else {
    sendSeriesButton_->disable();
    images_.clear();
  }
}
void DcmQRDBApplication::imageSelectionChanged(void) {
  if (imageTable_->selectedRows().size() > 0) sendImagesButton_->enable();
  else sendImagesButton_->disable();
}
void DcmQRDBApplication::sendStudies(void) {
  const std::vector< int > &rows = studyTable_->selectedRows();
  for(vector< int >::const_iterator i = rows.begin(); i != rows.end(); i++) {
    StudyData &stData = studies_[*i];
    sender_->queueJob( StudyLevel, stData.getUID(), 
      str( format("%1% Study[%2%]: %3%") 
	% stData.getFromTag( DCM_PatientsName ) 
	% stData.getFromTag( DCM_StudyID )
	% stData.getFromTag( DCM_StudyDescription )  ),
      localAETitle_, selectedDestinationPeer_, index_);
  }
}
void DcmQRDBApplication::sendSeries(void) {
  const std::vector< int > &rows = serieTable_->selectedRows();
  for(vector< int >::const_iterator i = rows.begin(); i != rows.end(); i++) {
    SerieData &serData = series_[*i];
    sender_->queueJob( SerieLevel, serData.getUID(),
      str( format("%1% Study[%2%]: %3% %4%-Series[%5%]:%6%") 
	% serData.getFromTag( DCM_PatientsName ) 
	% serData.getFromTag( DCM_StudyID )
	% serData.getFromTag( DCM_StudyDescription )
	% serData.getFromTag( DCM_Modality ) % serData.getFromTag( DCM_SeriesNumber ) % serData.getFromTag( DCM_SeriesDescription )),
      localAETitle_, selectedDestinationPeer_, index_);
  }
}
void DcmQRDBApplication::sendImages(void) {
  const std::vector< int > &rows = imageTable_->selectedRows();
  for(vector< int >::const_iterator i = rows.begin(); i != rows.end(); i++) {
    ImageData &imData = images_[*i];
    sender_->queueJob( ImageLevel, imData.getUID(),
      str( format("%1% Study[%2%]: %3% %4%-Series[%5%]:%6% Image[%7%]") 
	% imData.getFromTag( DCM_PatientsName ) 
	% imData.getFromTag( DCM_StudyID )
	% imData.getFromTag( DCM_StudyDescription )
	% imData.getFromTag( DCM_Modality ) % imData.getFromTag( DCM_SeriesNumber ) % imData.getFromTag( DCM_SeriesDescription )
	% imData.getFromTag( DCM_InstanceNumber )),
      localAETitle_, selectedDestinationPeer_, index_);
  }
}


void DcmQRDBApplication::searchIndex( bool background )
{
  wstring wsearchString = searchEdit_->text().value();
  string searchString;
  wstring2string( wsearchString, searchString );
  trim(searchString);
  int numCharacters = false;
  const int MinCharactersToWildcard = 3;
  for(string::const_iterator i = searchString.begin(); i != searchString.end(); i++) if (*i != '*' && *i != '?') numCharacters++;
  if (numCharacters >= MinCharactersToWildcard) {
    if (searchString[0] != '*') searchString = "*" + searchString;
    if (searchString[searchString.length()-1] != '*') searchString += "*";
  }
  if (background && numCharacters < MinCharactersToWildcard && searchString[0]=='*') {
    searchStatus_->setText( "leading Wildcard-Searches are -- S L O W -- press the Search Button to go for it" );
    return;
  }
  else {
    bool success = true;
    try {
      index_->findStudies(searchString, studies_);
    } catch (std::exception &e) {
      searchStatus_->setText( e.what() );
      success = false;
    }
    if (success) {
      searchStatus_->setText( str( format( "found %1% studies" ) % studies_.size()));
    }
    studyTable_->setModel( &studies_ );
  }
}

WApplication *createApplication(const WEnvironment& env)
{
  return new DcmQRDBApplication(env, myConfig, myIndexDispatcher, mySender);
}

int main(int argc, char **argv)
{
  const char dcmqrscpConfigPathEnv[] = "DCMQRSCP_CONFIG";
  char *dcmqrscpConfigPath = getenv("DCMQRSCP_CONFIG");
  if (dcmqrscpConfigPath == NULL) {
    cerr << "Warning: did not find Environment-Variable " << dcmqrscpConfigPathEnv << " specifying the path of dcmqrscp.cfg" << endl;
    exit(1);
  }
  myConfig.reset(new DicomConfig(dcmqrscpConfigPath));
  myIndexDispatcher.reset(new IndexDispatcher);
  mySender.reset(new Sender);
  return WRun(argc, argv, &createApplication);
}

