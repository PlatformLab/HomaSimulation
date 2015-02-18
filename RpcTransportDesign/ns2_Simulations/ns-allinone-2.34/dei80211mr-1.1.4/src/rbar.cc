/*
 * Copyright (c) 2007 Regents of the SIGNET lab, University of Padova.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Padova (SIGNET lab) nor the 
 *    names of its contributors may be used to endorse or promote products 
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "rbar.h"
#include"mac-802_11mr.h"

#include<otcl.h>

static class RBARClass : public TclClass {
public:
	RBARClass() : TclClass("RateAdapter/RBAR") {}
	TclObject* create(int, const char*const*) {
		return (new RBAR);
	}
} class_snr_rate_adapter;



RBAR::RBAR() 
{
 
}

RBAR::~RBAR()
{
}


int RBAR::command(int argc, const char*const* argv)
{
  Tcl& tcl = Tcl::instance();

if(argc == 4)
    {
      if (strcasecmp(argv[1], "phymode") == 0) 
	{
	  cerr << "Error in RBAR::command \"phymode\" : command requires further argument (SNR threshold) " << endl;
	  return TCL_ERROR;
	}	  
    } 
 else if(argc == 5)
    {
      if (strcasecmp(argv[1], "phymode") == 0) 
	{
	  PhyMode mode = str2PhyMode(argv[2]);

	  if(mode == ModeUnknown)
	    {
	      cerr << "Error in RateAdapter::command(\"phymode\","<< argv[2] << "," << argv[3] << "," << argv[4] << ") : unknown PHY mode \"" << argv[2]  <<"\"" <<endl;	      
	      return TCL_ERROR;
	    }

	  int index = atoi(argv[3]);
	  if ((index < 0) || (index >= RA_MAX_NUM_PHY_MODES))
	    {
	      cerr << "Error in  RateAdapter::command(\"phymode\","<< argv[2] << "," << argv[3] << "," << argv[4] << ") : phymode index out of bounds" << endl;
	      return TCL_ERROR;
	    }

	  double thresh = atof(argv[4]);
	  modes[index] = mode;
	  snr_thresholds[index] = thresh;
	  
	  return TCL_OK;	    
	}	  
    }



  // Fallback for unknown commands
  return RateAdapter::command(argc, argv);
}


void RBAR::sendDATA(Packet* p)
{

  hdr_cmn* ch = HDR_CMN(p);
  MultiRateHdr *mrh = HDR_MULTIRATE(p);
  struct hdr_mac802_11* dh = HDR_MAC802_11(p);

  u_int32_t dst = (u_int32_t)ETHER_ADDR(dh->dh_ra);
  u_int32_t src = (u_int32_t)(mac_->addr());


  if((u_int32_t)ETHER_ADDR(dh->dh_ra) == MAC_BROADCAST) 
    {
      // Broadcast packets are always sent at the basic rate
      return;
    }


  double snr =  getSNR(p); 

  assert(getPER());
  
  int idx = 0; 

  while ((idx < numPhyModes_-1) && (snr > snr_thresholds[idx+1])) 
    {
      idx++;      
    }
  
  setModeAtIndex(idx);
}



double RBAR::getSNR(Packet* p)
{
  struct hdr_mac802_11* dh = HDR_MAC802_11(p);
  
  u_int32_t dst = (u_int32_t)ETHER_ADDR(dh->dh_ra);
  u_int32_t src = (u_int32_t)(mac_->addr());

  return getPeerStats(src, dst)->getSnr();  
}
