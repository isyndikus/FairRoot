/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             * 
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *  
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/
// -------------------------------------------------------------------------
// -----                   FairRunOnline source file                   -----
// -----            Created 06/01/04  by M. Al-Turany                  -----
// -------------------------------------------------------------------------

#include "FairRunOnline.h"

#include "FairRootManager.h"
#include "FairTask.h"
#include "FairBaseParSet.h"
#include "FairEventHeader.h"
#include "FairFieldFactory.h"
#include "FairRuntimeDb.h"
#include "FairRunIdGenerator.h"
#include "FairLogger.h"
#include "FairFileHeader.h"
#include "FairParIo.h"
#include "FairField.h"
//#include "FairSource.h"
#include "FairMbsSource.h"

#include "FairGeoInterface.h"
#include "FairGeoLoader.h"
#include "FairGeoParSet.h"

#include "TROOT.h"
#include "TSystem.h"
#include "TTree.h"
#include "TSeqCollection.h"
#include "TGeoManager.h"
#include "TKey.h"
#include "TF1.h"
#include "TSystem.h"
#include "TFolder.h"
#include "TH1F.h"
#include "TH2F.h"
#include "THttpServer.h"

#include <signal.h>
#include <stdlib.h>
#include <iostream>
#include <list>

using std::cout;
using std::endl;
using std::list;



//_____________________________________________________________________________
FairRunOnline* FairRunOnline::fgRinstance = 0;



//_____________________________________________________________________________
FairRunOnline* FairRunOnline::Instance()
{
  return fgRinstance;
}


FairRunOnline::FairRunOnline()
  :FairRun(),
   fAutomaticFinish(kTRUE),
   fIsInitialized(kFALSE),
   fStatic(kFALSE),
   fField(0),
   fNevents(0),
   fServer(NULL),
   fServerRefreshRate(0)
{
  fgRinstance = this;
  fAna = kTRUE;
  LOG(INFO) << "FairRunOnline constructed at " << this << FairLogger::endl;
}
//_____________________________________________________________________________
FairRunOnline::FairRunOnline(FairSource* source)
  :FairRun(),
   fAutomaticFinish(kTRUE),
   fIsInitialized(kFALSE),
   fStatic(kFALSE),
   fField(0),
   fNevents(0),
   fServer(NULL),
   fServerRefreshRate(0)
{
  fRootManager->SetSource(source);
  fgRinstance = this;
  fAna = kTRUE;
  LOG(INFO) << "FairRunOnline constructed at " << this << FairLogger::endl;
}
//_____________________________________________________________________________

//_____________________________________________________________________________
FairRunOnline::~FairRunOnline()
{
  //  delete fFriendFileList;
  delete fField;
  if (gGeoManager) {
    if (gROOT->GetVersionInt() >= 60602) {
      gGeoManager->GetListOfVolumes()->Delete();
      gGeoManager->GetListOfShapes()->Delete();
    }
    delete gGeoManager;
  }
  delete fServer;
}
//_____________________________________________________________________________



Bool_t gIsInterrupted;

void handler_ctrlc(int)
{
  gIsInterrupted = kTRUE;
}



//_____________________________________________________________________________
void FairRunOnline::Init()
{
  LOG(INFO)<<"FairRunOnline::Init"<<FairLogger::endl;
  if (fIsInitialized) {
    LOG(FATAL) << "Error Init is already called before!" << FairLogger::endl;
    exit(-1);
  } else {
    fIsInitialized = kTRUE;
  }

  fRootManager->InitSource();

  //  FairGeoLoader* loader = new FairGeoLoader("TGeo", "Geo Loader");
  //  FairGeoInterface* GeoInterFace = loader->getGeoInterface();
  //  GeoInterFace->SetNoOfSets(ListOfModules->GetEntries());
  //  GeoInterFace->setMediaFile(MatFname.Data());
  //  GeoInterFace->readMedia();

  // Add a Generated run ID to the FairRunTimeDb for input data which contain a FairMCEventHeader
  // The call doesn't make sense for online sources which doesn't contain a  FairMCEventHeader

  if(kONLINE != fRootManager->GetSource()->GetSourceType())
  {
    fRootManager->ReadEvent(0);
  }

  GetEventHeader();
  
  fRootManager->FillEventHeader(fEvtHeader);

  if(0 == fRunId) // Run ID was not set in run manager
  {
    if(0 == fEvtHeader->GetRunId()) // Run ID was not set in source
    {
      // Generate unique Run ID
      FairRunIdGenerator genid;
      fRunId = genid.generateId();
      fRootManager->GetSource()->SetRunId(fRunId);
    }
    else
    {
      // Use Run ID from source
      fRunId = fEvtHeader->GetRunId();
    }
  }
  else
  {
    // Run ID was set in the run manager - propagate to source
    fRootManager->GetSource()->SetRunId(fRunId);
  }

  fRtdb->addRun(fRunId);
  fFileHeader->SetRunId(fRunId);
  FairBaseParSet* par = dynamic_cast<FairBaseParSet*>(fRtdb->getContainer("FairBaseParSet"));
  FairGeoParSet* geopar = dynamic_cast<FairGeoParSet*>(fRtdb->getContainer("FairGeoParSet"));
  if (geopar) {
    geopar->SetGeometry(gGeoManager);
  }
  if(fField) {
    fField->Init();
    fField->FillParContainer();
  }
  TList* containerList = fRtdb->getListOfContainers();
  TIter next(containerList);
  FairParSet* cont;
  TObjArray* ContList= new TObjArray();
  while ((cont=dynamic_cast<FairParSet*>(next()))) {
    ContList->Add(new TObjString(cont->GetName()));
  }
  if (par) {
    par->SetContListStr(ContList);
    par->setChanged();
    par->setInputVersion(fRunId,1);
  }
  if (geopar) {
    geopar->setChanged();
    geopar->setInputVersion(fRunId,1);
  }
  
  fRootManager->WriteFileHeader(fFileHeader);

  fRootManager->GetSource()->SetParUnpackers();
  fTask->SetParTask();
  fRtdb->initContainers(fRunId);

  //  InitContainers();
  // --- Get event header from Run
  if ( ! fEvtHeader ) {
    LOG(FATAL) << "FairRunOnline::InitContainers:No event header in run!" << FairLogger::endl;
    return;
  }
  LOG(INFO) << "FairRunOnline::InitContainers: event header at " << fEvtHeader << FairLogger::endl;
  fRootManager->Register("EventHeader.", "Event", fEvtHeader, kTRUE);
  fEvtHeader->SetRunId(fRunId);

  fRootManager->GetSource()->InitUnpackers();

  // Now call the User initialize for Tasks
  fTask->InitTask();

  // create the output tree after tasks initialisation
  fOutFile->cd();
  TTree* outTree =new TTree(FairRootManager::GetTreeName(), "/cbmout", 99);
  fRootManager->TruncateBranchNames(outTree, "cbmout");
  fRootManager->SetOutTree(outTree);
  fRootManager->WriteFolder();
}
//_____________________________________________________________________________


//_____________________________________________________________________________
void FairRunOnline::InitContainers()
{

  fRtdb = GetRuntimeDb();
  FairBaseParSet* par=static_cast<FairBaseParSet*>
                      (fRtdb->getContainer("FairBaseParSet"));
  LOG(INFO) << "FairRunOnline::InitContainers: par = " << par << FairLogger::endl;
  if (NULL == par)
    LOG(WARNING)<<"FairRunOnline::InitContainers: no  'FairBaseParSet' container !"<<FairLogger::endl;

  if (par) {
    fEvtHeader = static_cast<FairEventHeader*>(fRootManager->GetObject("EventHeader."));

    fRunId = fEvtHeader->GetRunId();

    //Copy the Event Header Info to Output
    fEvtHeader->Register();

    // Init the containers in Tasks
    fRtdb->initContainers(fRunId);
    fTask->ReInitTask();
    //    fTask->SetParTask();
    fRtdb->initContainers( fRunId );
    //     if (gGeoManager==0) {
    //       par->GetGeometry();
    //     }
  } else
    {
      // --- Get event header from Run
      //      fEvtHeader = dynamic_cast<FairEventHeader*> (FairRunOnline::Instance()->GetEventHeade
      GetEventHeader();
      if ( ! fEvtHeader ) {
	LOG(FATAL) << "FairRunOnline::InitContainers:No event header in run!" << FairLogger::endl;
	return;
      }
      LOG(INFO) << "FairRunOnline::InitContainers: event header at " << fEvtHeader << FairLogger::endl;
      fRootManager->Register("EventHeader.", "Event", fEvtHeader, kTRUE);
    }
}
//_____________________________________________________________________________
Int_t FairRunOnline::EventLoop()
{
  gSystem->IgnoreInterrupt();
  signal(SIGINT, handler_ctrlc);

  fRootManager->FillEventHeader(fEvtHeader);
  Int_t tmpId = fEvtHeader->GetRunId();

  if ( tmpId != -1 && tmpId != fRunId ) {
    LOG(INFO) << "FairRunOnline::EventLoop() Call Reinit due to changed RunID (from " << fRunId << " to " << tmpId << FairLogger::endl;
    fRunId = tmpId;
    Reinit( fRunId );
    fRootManager->GetSource()->ReInitUnpackers();
    fTask->ReInitTask();
  }

  fRootManager->StoreWriteoutBufferData(fRootManager->GetEventTime());
  fTask->ExecuteTask("");
  fRootManager->FillEventHeader(fEvtHeader);
  Fill();
  fRootManager->DeleteOldWriteoutBufferData();
  fTask->FinishEvent();
  fNevents += 1;
  if(fServer && 0 == (fNevents%fServerRefreshRate))
  {
    fServer->ProcessRequests();
  }
    
  if(gIsInterrupted)
  {
    return 1;
  }

  return 0;
}

//_____________________________________________________________________________
void FairRunOnline::Run(Int_t Ev_start, Int_t Ev_end)
{
  fOutFile->cd();
  
  fNevents = 0;

  gIsInterrupted = kFALSE;
  
  Int_t MaxAllowed=fRootManager->CheckMaxEventNo(Ev_end);
  if ( MaxAllowed != -1 ) {
    if (Ev_end==0) {
      if (Ev_start==0) {
        Ev_end=MaxAllowed;
      } else {
        Ev_end =  Ev_start;
        if ( Ev_end > MaxAllowed ) {
          Ev_end = MaxAllowed;
        }
        Ev_start=0;
      }
    } else {
      if (Ev_end > MaxAllowed) {
        cout << "-------------------Warning---------------------------" << endl;
        cout << " -W FairRunAna : File has less events than requested!!" << endl;
        cout << " File contains : " << MaxAllowed  << " Events" << endl;
        cout << " Requested number of events = " <<  Ev_end <<  " Events"<< endl;
        cout << " The number of events is set to " << MaxAllowed << " Events"<< endl;
        cout << "-----------------------------------------------------" << endl;
        Ev_end = MaxAllowed;
      }
    }
  }
  Int_t status;
  if(Ev_start < 0) {
    while(kTRUE) {
      status = fRootManager->ReadEvent();
      if(0 == status) {
	status = EventLoop();
      }
      if(1 == status) {
        break;
      } else if(2 == status) {
        continue;
      }
      if(gIsInterrupted)
      {
        break;
      }
    }
  } else {
    for (Int_t i = Ev_start; i < Ev_end; i++) {
      status = fRootManager->ReadEvent(i);
      if(0 == status) {
	status = EventLoop();
      }
      if(1 == status) {
        break;
      } else if(2 == status) {
        i -= 1;
        continue;
      }

      if(gIsInterrupted)
      {
        break;
      }
    }
  }
 
  fRootManager->StoreAllWriteoutBufferData();
  if (fAutomaticFinish) {
    Finish();
  }
}
//_____________________________________________________________________________

//_____________________________________________________________________________
void FairRunOnline::Finish()
{
  fTask->FinishTask();
  fRootManager->LastFill();
  fRootManager->Write();
  fRootManager->GetSource()->Close();

  fRootManager->CloseOutFile();
}
//_____________________________________________________________________________

//_____________________________________________________________________________
void FairRunOnline::ActivateHttpServer(Int_t refreshRate, Int_t httpServer)
{
  TString serverAddress="http:";
  serverAddress+=httpServer;
  fServer = new THttpServer(serverAddress);
  fServerRefreshRate = refreshRate;
}
//_____________________________________________________________________________


//_____________________________________________________________________________
void FairRunOnline::RegisterHttpCommand(TString name, TString command)
{
  if(fServer)
  {
    TString path = "/Objects/HISTO";
    fServer->RegisterCommand(name, path + command);
  }
}
//_____________________________________________________________________________


//_____________________________________________________________________________
void FairRunOnline::Reinit(UInt_t runId)
{
  // reinit procedure
  fRtdb->initContainers( runId );
}
//_____________________________________________________________________________



//_____________________________________________________________________________
void  FairRunOnline::SetContainerStatic(Bool_t tempBool)
{
  fStatic=tempBool;
  if ( fStatic ) {
    LOG(INFO) << "Parameter Cont. initialisation is static" 
	      << FairLogger::endl;
  } else {
    LOG(INFO) << "Parameter Cont. initialisation is NOT static"
	      << FairLogger::endl;
  }
}
//_____________________________________________________________________________



//_____________________________________________________________________________
void FairRunOnline::AddObject(TObject* object)
{
    if(NULL == object)
    {
        return;
    }
    if(fServer) {
        TString classname = TString(object->ClassName());
        if(classname.EqualTo("TCanvas"))
        {
            fServer->Register("CANVAS", object);
        }
        else if(classname.EqualTo("TFolder"))
        {
            fServer->Register("/", object);
        }
        else if(classname.Contains("TH1") || classname.Contains("TH2"))
        {
            fServer->Register("HISTO", object);
        }
        else
        {
            LOG(WARNING) << "FairRunOnline::AddObject : unrecognized object type : " << classname << FairLogger::endl;
        }
    }
}
//_____________________________________________________________________________



//_____________________________________________________________________________
void FairRunOnline::Fill()
{
  if(fMarkFill)
  {
    fRootManager->Fill();
  }
  else
  {
    fMarkFill = kTRUE;
  }
}
//_____________________________________________________________________________



ClassImp(FairRunOnline)
