/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *         GNU Lesser General Public Licence version 3 (LGPL) version 3,        *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// ROOT headers
#include "TClonesArray.h"

// Fair headers
#include "FairRootManager.h"
#include "FairRunOnline.h"
#include "FairLogger.h"

// Land headers
#include "FairMBSRawItem.h"
#include "FairMBSUnpack.h"

// FairMBSUnpack: Constructor
FairMBSUnpack::FairMBSUnpack(Short_t type, Short_t subType, Short_t procId, Short_t subCrate, Short_t control)
    : FairUnpack(type, subType, procId, subCrate, control)
    , fRawData(new TClonesArray("FairMBSRawItem"))
    , fNHits(0)
    , fNHitsTotal(0)
{
}

// Virtual FairMBSUnpack: Public method
FairMBSUnpack::~FairMBSUnpack()
{
    LOG(INFO) << "FairMBSUnpack: Delete instance" << FairLogger::endl;
    delete fRawData;
}

// Init: Public method
Bool_t FairMBSUnpack::Init()
{
    Register();
    return kTRUE;
}

// Register: Protected method
void FairMBSUnpack::Register()
{
    //  LOG(DEBUG) << "Registering" << FairLogger::endl;
    LOG(INFO) << "FairMBSUnpack : Registering..." << FairLogger::endl;
    FairRootManager* fMan = FairRootManager::Instance();
    if (!fMan)
    {
        return;
    }
    fMan->Register("MBSRawItem", "MBS", fRawData, kTRUE);
}

// DoUnpack: Public method
Bool_t FairMBSUnpack::DoUnpack(Int_t* data, Int_t size)
{
    LOG(DEBUG) << "FairMBSUnpack : Unpacking... size = " << size << FairLogger::endl;

    Int_t l_i = 0;

    Int_t n17 = 0;

    while (l_i < size)
    {
        n17 = 0;

        UInt_t* p1 = reinterpret_cast<UInt_t*>(data + l_i);
        UInt_t l_sam_id = (p1[0] & 0xf0000000) >> 28; // identifies the sam
        UInt_t l_gtb_id = (p1[0] & 0x0f000000) >> 24; // 0 or 1, identifies which of the 2 cables of the sam
        UInt_t l_lec = (p1[0] & 0x00f00000) >> 20;
        UInt_t l_da_siz = (p1[0] & 0x000001ff);

        LOG(DEBUG) << "FairMBSUnpack : SAM:" << l_sam_id << ",  GTB:" << l_gtb_id << ",  lec:" << l_lec
                   << ",  size:" << l_da_siz << FairLogger::endl;

        l_i += 1;

        p1 = reinterpret_cast<UInt_t*>((data + l_i));

        for (UInt_t i1 = 0; i1 < l_da_siz; i1 += 2)
        {
            UInt_t tac_addr;
            UInt_t tac_ch;
            UInt_t cal;
            UInt_t clock;
            UInt_t tac_data;
            UInt_t qdc_data;
            tac_addr = (p1[i1] & 0xf8000000) >> 27;
            tac_ch = (p1[i1] & 0x07c00000) >> 22;
            cal = (p1[i1] & 0x003C0000) >> 18;
            clock = 63 - ((p1[i1] & 0x0003f000) >> 12);
            tac_data = 4095 - ((p1[i1] & 0x00000fff));
            qdc_data = (p1[i1 + 1] & 0x00000fff);
            l_i += 2;

            if (16 == tac_ch)
            {
                n17 += 1;
            }
            LOG(DEBUG) << "FairMBSUnpack : TAC ADDR IS " << tac_addr << ",  TAC CH IS " << tac_ch << ",  TAC Data IS "
                       << tac_data << ",  QDC Data IS " << qdc_data << FairLogger::endl;
            new ((*fRawData)[fNHits])
                FairMBSRawItem(l_sam_id, l_gtb_id, tac_addr, tac_ch, cal, clock, tac_data, qdc_data);
            fNHits++;
        }

        LOG(DEBUG) << "FairMBSUnpack : n17=" << n17 << FairLogger::endl;
    }

    LOG(DEBUG) << "FairMBSUnpack : Number of hits in LAND: " << fNHits << FairLogger::endl;
    
    fNHitsTotal += fNHits;
    
    return kTRUE;
}

// Reset: Public method
void FairMBSUnpack::Reset()
{
    LOG(DEBUG) << "FairMBSUnpack : Clearing Data Structure" << FairLogger::endl;
    fRawData->Clear();
    fNHits = 0;
}

ClassImp(FairMBSUnpack)
