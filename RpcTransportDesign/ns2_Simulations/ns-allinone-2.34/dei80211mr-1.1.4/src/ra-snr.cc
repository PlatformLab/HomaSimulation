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


#include "ra-snr.h"
#include"mac-802_11mr.h"

static class SnrRateAdapterClass : public TclClass {
public:
	SnrRateAdapterClass() : TclClass("RateAdapter/SNR") {}
	TclObject* create(int, const char*const*) {
		return (new SnrRateAdapter);
	}
} class_snr_rate_adapter;



SnrRateAdapter::SnrRateAdapter() 
{
  bind("maxper_", &maxper_); 
  bind("pktsize_", &pktsize_); 
}

SnrRateAdapter::~SnrRateAdapter()
{
}


int SnrRateAdapter::command(int argc, const char*const* argv)
{
 
  // Add TCL commands here...

  // Fallback for unknown commands
  return RateAdapter::command(argc, argv);
}


void SnrRateAdapter::sendDATA(Packet* p)
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
  PhyMode pm = getModeAtIndex(idx);
  double per = 0;

  while ( ((idx+1) < numPhyModes_) && (per<maxper_))
    {
      pm = getModeAtIndex(idx + 1);
      per = getPER()->get_per(pm, snr, pktsize_);

      if (per<maxper_)
	idx++;      
    }
  
  setModeAtIndex(idx);
}



double SnrRateAdapter::getSNR(Packet* p)
{
  struct hdr_mac802_11* dh = HDR_MAC802_11(p);
  
  u_int32_t dst = (u_int32_t)ETHER_ADDR(dh->dh_ra);
  u_int32_t src = (u_int32_t)(mac_->addr());

  return getPeerStats(src, dst)->getSnr();  
}
