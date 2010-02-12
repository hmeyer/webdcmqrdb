/*
 * Copyright (C) 2008 Emweb bvba, Heverlee, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include <Wt/WApplication>
#include <Wt/WContainerWidget>
#include <Wt/WLayout>


#include <Wt/Ext/LineEdit>
#include <Wt/Ext/Button>
#include <Wt/Ext/TableView>
#include <Wt/Ext/ToolBar>
#include <Wt/Ext/TabWidget>

#include <boost/lexical_cast.hpp>


#include "index.h"
#include "sender.h"

#include <algorithm>
#include <string>
#include <boost/range/end.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>

#include "dcmtk/dcmdata/dcdeftag.h"


using namespace Wt;
using namespace std;
using namespace boost;

void wstring2string( const wstring &w, string &s) {
  s.assign( w.begin(), w.end() );
}

typedef shared_ptr< Index > IndexPtr;
typedef shared_ptr< DicomSender > SenderPtr;

  IndexPtr myIndex;
  SenderPtr mySender;

/*
 * A simple hello world application class which demonstrates how to react
 * to events, read input, and give feed-back.
 */
class DcmQRDBApplication : public WApplication
{
public:
  DcmQRDBApplication(const WEnvironment& env, IndexPtr index, SenderPtr sender );

protected:

private:
  Ext::LineEdit *searchEdit_;
  Ext::Button *sendStudiesButton_;
  Ext::Button *sendSeriesButton_;
  Ext::Button *sendImagesButton_;
  Ext::TableView *studyTable_;
  Ext::TableView *serieTable_;
  Ext::TableView *imageTable_;
  IndexPtr index_;
  SenderPtr sender_;
  StudyList studies_;
  SerieList series_;
  ImageList images_;
  void searchIndex( bool background = false );
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
DcmQRDBApplication::DcmQRDBApplication(const WEnvironment& env, IndexPtr index, SenderPtr sender)
  : WApplication(env), index_(index), sender_(sender)
{
  setTitle("Dicom-Web-DataBase");                               // application title
  
  Ext::TabWidget *tabs = new Ext::TabWidget();
  Ext::Panel *searchPanel = new Ext::Panel(); searchPanel->setTitle("Search");
  Ext::Panel *jobsPanel = new Ext::Panel(); jobsPanel->setTitle("Jobs");
  Ext::Panel *configPanel = new Ext::Panel(); configPanel->setTitle("Config");
  tabs->addTab( searchPanel );
  tabs->addTab( jobsPanel );
  tabs->addTab( configPanel );
  
  root()->addWidget( tabs );

  searchEdit_ = new Ext::LineEdit();                     // allow text input
  searchEdit_->setFocus();                                 // give focus
  root()->addWidget( searchEdit_ );

  Ext::Button *b = new Ext::Button("Search"); // create a button
  b->setMargin(5, Left);                                 // add 5 pixels margin
  root()->addWidget( b );

  root()->addWidget(new WBreak() );                       // insert a line break

  studyTable_ = new Ext::TableView();
  studyTable_->setModel( &studies_ );
  studyTable_->setDataLocation(Ext::ServerSide);
  studyTable_->setSelectionBehavior(SelectRows);
  studyTable_->setSelectionMode(ExtendedSelection);
  studyTable_->itemSelectionChanged().connect( boost::bind(&DcmQRDBApplication::studySelectionChanged, this) );
  studyTable_->setPageSize(10);
  studyTable_->setBottomToolBar(studyTable_->createPagingToolBar());
  sendStudiesButton_ = new Ext::Button("Send");
  sendStudiesButton_->disable();
  sendStudiesButton_->clicked().connect( boost::bind(&DcmQRDBApplication::sendStudies, this) );
  studyTable_->bottomToolBar()->add( sendStudiesButton_ );
  studyTable_->resize( WLength::Auto, WLength(16, WLength::FontEm) );
  root()->addWidget( studyTable_ );

  serieTable_ = new Ext::TableView();
  serieTable_->setModel( &series_ );
  serieTable_->setDataLocation(Ext::ServerSide);
  serieTable_->setSelectionBehavior(SelectRows);
  serieTable_->setSelectionMode(ExtendedSelection);
  serieTable_->itemSelectionChanged().connect( boost::bind(&DcmQRDBApplication::serieSelectionChanged, this) );
  serieTable_->setPageSize(5);
  serieTable_->setBottomToolBar(serieTable_->createPagingToolBar());
  sendSeriesButton_ = new Ext::Button("Send");
  sendSeriesButton_->disable();
  sendSeriesButton_->clicked().connect( boost::bind(&DcmQRDBApplication::sendSeries, this) );
  serieTable_->bottomToolBar()->add( sendSeriesButton_ );
  serieTable_->resize( WLength::Auto, WLength(9, WLength::FontEm) );
  root()->addWidget( serieTable_ );

  imageTable_ = new Ext::TableView();
  imageTable_->setModel( &images_ );
  imageTable_->setDataLocation(Ext::ServerSide);
  imageTable_->setSelectionBehavior(SelectRows);
  imageTable_->setSelectionMode(ExtendedSelection);
  imageTable_->itemSelectionChanged().connect( boost::bind(&DcmQRDBApplication::imageSelectionChanged, this) );
  imageTable_->setPageSize(5);
  imageTable_->setBottomToolBar(imageTable_->createPagingToolBar());
  sendImagesButton_ = new Ext::Button("Send");
  sendImagesButton_->disable();
  sendImagesButton_->clicked().connect( boost::bind(&DcmQRDBApplication::sendImages, this) );
  imageTable_->bottomToolBar()->add( sendImagesButton_ );
  imageTable_->resize( WLength::Auto, WLength(9, WLength::FontEm) );
  root()->addWidget( imageTable_ );  
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
}



void DcmQRDBApplication::studySelectionChanged(void) {
  const std::vector< int > &rows = studyTable_->selectedRows();
  if (rows.size() > 0) {
    sendStudiesButton_->enable();
    vector< string > stlist;
    for(vector< int >::const_iterator i = rows.begin(); i != rows.end(); i++)
      stlist.push_back( studies_.getUID(*i) );
    index_->getSeries( stlist, series_ );
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
    vector< string > selist;
    for(vector< int >::const_iterator i = rows.begin(); i != rows.end(); i++)
      selist.push_back( series_.getUID(*i) );
    index_->getImages( selist, images_ );
    imageTable_->setModel( &images_ );
  }
  else {
    sendSeriesButton_->disable();
    images_.clear();
  }
}
void DcmQRDBApplication::imageSelectionChanged(void) {
  if (imageTable_->selectedRows().size() > 0) sendSeriesButton_->enable();
  else sendSeriesButton_->disable();
}
void DcmQRDBApplication::sendStudies(void) {
  const std::vector< int > &rows = studyTable_->selectedRows();
  for(vector< int >::const_iterator i = rows.begin(); i != rows.end(); i++) {
    StudyData &stData = studies_[*i];
    sender_->queueJob( StudyLevel, stData.getUID(), 
      str( format("%1% Study[%2%]: %3%") 
	% stData.getFromTag( DCM_PatientsName ) 
	% stData[3] // TODO: replace with tags
	% stData[5])  );
  }
}
void DcmQRDBApplication::sendSeries(void) {
  const std::vector< int > &rows = serieTable_->selectedRows();
  for(vector< int >::const_iterator i = rows.begin(); i != rows.end(); i++) {
    SerieData &serData = series_[*i];
    sender_->queueJob( SerieLevel, serData.getUID(),
      str( format("%1% Study[%2%]: %3% %5%-Series[%4%]:%6%") 
	% serData[4] % serData[5] % serData[6] % serData[0] 
	% serData[1] % serData[2])  );
  }
}
void DcmQRDBApplication::sendImages(void) {
  const std::vector< int > &rows = imageTable_->selectedRows();
  for(vector< int >::const_iterator i = rows.begin(); i != rows.end(); i++) {
    ImageData &imData = images_[*i];
    sender_->queueJob( ImageLevel, imData.getUID(),
      str( format("%1% Study[%2%]: %3% %5%-Series[%4%]:%6% Image[%7%]") 
	% imData[3] % imData[4] % imData[5] % imData[7] 
	% imData[8] % imData[9] % imData[0])  );
  }
}


void DcmQRDBApplication::searchIndex( bool background )
{
  wstring wsearchString = searchEdit_->text().value();
  string searchString;
  wstring2string( wsearchString, searchString );
  if (!background || searchString!="*") {
    index_->findStudies(searchString, studies_);
    studyTable_->setModel( &studies_ );
  }
}

WApplication *createApplication(const WEnvironment& env)
{
  return new DcmQRDBApplication(env, myIndex, mySender);
}

int main(int argc, char **argv)
{
  myIndex.reset(new Index("/home/hmeyer/tmp/"));
  mySender.reset(new DicomSender());
  return WRun(argc, argv, &createApplication);
}

