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



#include"Mac80211EventHandler.h"

#include"mac-802_11mr.h"



#define NOT_IMPLEMENTED 0




Mac80211EventHandler::Mac80211EventHandler()
  : mac_(0), next(0)
{
}




Mac80211EventHandler::~Mac80211EventHandler()
{
  //delete old_MAC_MIB;
}



int Mac80211EventHandler::command(int argc, const char*const* argv)
{
  if(argc == 3)
    {
      if (strcasecmp(argv[1], "setmac") == 0) {
	mac_ = dynamic_cast<Mac802_11mr*>(TclObject::lookup(argv[2]));
	if (mac_ == 0)
	  return TCL_ERROR;
	return TCL_OK;
      }       
    }

  // Fallback for unknown commands
  return TclObject::command(argc, argv);
}





MR_MAC_MIB* Mac80211EventHandler::getMacMib()  
{
  assert(mac_);
  return &(mac_->macmib_);
}


// MR_MAC_MIB* Mac80211EventHandler::getOldMacMib()  
// {
//   return old_MAC_MIB;
// }



MR_PHY_MIB* Mac80211EventHandler::getPhyMib()
{
  assert(mac_);
  return &(mac_->phymib_);
}



PER* Mac80211EventHandler::getPER()
{
  assert(mac_);
  return mac_->per_;
}


PeerStats* Mac80211EventHandler::getPeerStats(u_int32_t src, u_int32_t dst)
{
  assert(mac_);
  assert(mac_->peerstatsdb_);
  return mac_->peerstatsdb_->getPeerStats(src, dst);
}


void Mac80211EventHandler::counterHandle(CounterId id)
{
  counterEvent(id);
  //  *old_MAC_MIB = *getMacMib();
}

void Mac80211EventHandler::counterEvent(CounterId id)
{
}

void Mac80211EventHandler::chBusy(double t)
{
}

void Mac80211EventHandler::sendDATA(Packet* p)
{
}


PhyMode Mac80211EventHandler::getBasicMode()
{
  return mac_->basicMode_;

}


PhyMode Mac80211EventHandler::getDataMode()
{
  return mac_->dataMode_;
}





void Mac80211EventHandler::setBasicMode(PhyMode m)
{
  assert(m != ModeUnknown);
  assert(mac_);
  mac_->basicMode_ = m;
}

void Mac80211EventHandler::setDataMode(PhyMode m)
{
  assert(m != ModeUnknown);
  assert(mac_);
  mac_->dataMode_ = m;
}
  

void Mac80211EventHandler::setRTSThreshold(u_int32_t value)
{
  mac_->macmib_.setRTSThreshold(value);

}


void Mac80211EventHandler::setShortRetryLimit(u_int32_t value)
{
  mac_->macmib_.setShortRetryLimit(value);
}


void Mac80211EventHandler::setLongRetryLimit(u_int32_t value)
{
  mac_->macmib_.setLongRetryLimit(value);
}


void Mac80211EventHandler::setCSThreshold(u_int32_t value)
{
  assert(NOT_IMPLEMENTED);
}




