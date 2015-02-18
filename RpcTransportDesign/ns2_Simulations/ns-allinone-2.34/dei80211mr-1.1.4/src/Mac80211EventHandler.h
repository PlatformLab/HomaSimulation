/* -*- Mode:C++ -*- */

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


#ifndef MAC80211EVENTHANDLER_H
#define MAC80211EVENTHANDLER_H


#include<phymodes.h>
#include<peerstats.h>


//#include<mac-802_11mr.h>
class Mac802_11mr;
class MR_MAC_MIB;
class MR_PHY_MIB;
class PER;




/**
 * @todo this scheme is nice but does not currently allow for distinction
 * between, e.g., FrameReceives and DataFrameReceives. The reason is
 * that in mac-802_11mr.cc a single counter event is triggered for
 * both. Either we trigger distinct counter events (somehow better) or
 * we use kind of a bitmap for identifying several counter events at
 * once (which is more complex).
 * 
 */
typedef enum {
	ID_Unknown = 0,
	ID_MPDUTxSuccessful, ID_MPDUTxOneRetry, ID_MPDUTxMultipleRetries, 
	ID_MPDUTxFailed, ID_RTSFailed, ID_ACKFailed, 
	ID_MPDURxSuccessful, ID_FrameReceives, ID_DataFrameReceives, ID_CtrlFrameReceives, ID_MgmtFrameReceives, 
	ID_FrameErrors, ID_DataFrameErrors, ID_CtrlFrameErrors, ID_MgmtFrameErrors, 
	ID_FrameErrorsNoise, ID_DataFrameErrorsNoise, ID_CtrlFrameErrorsNoise, ID_MgmtFrameErrorsNoise, 
	ID_FrameDropSyn, ID_DataFrameDropSyn, ID_CtrlFrameDropSyn, ID_MgmtFrameDropSyn, 
	ID_FrameDropTxa, ID_DataFrameDropTxa, ID_CtrlFrameDropTxa, ID_MgmtFrameDropTxa, 
	ID_idleSlots, ID_idle2Slots
} CounterId;



/**
 * This class provides an interface for developing plugins which react
 * to MAC events and perform tunings of MAC parameters 
 * 
 */
class Mac80211EventHandler : public TclObject
{

  friend class Mac802_11mr;

public:

  Mac80211EventHandler();
  virtual ~Mac80211EventHandler();

  /** 
   * Entry point for counter events, to be overloaded by
   * derived classes
   *
   * @param id the id of the counter which has just been updated
   * 
   */
  virtual void counterEvent(CounterId id);


  /** 
   * This method is called when the mac is just to occupy the medium for t seconds
   * 
   * @param t 
   */
  virtual void chBusy(double t); 


  virtual void sendDATA(Packet* p);


  void setBasicMode(PhyMode m);

  void setDataMode(PhyMode m);
  
  void setRTSThreshold(u_int32_t value);

  void setShortRetryLimit(u_int32_t value);

  void setLongRetryLimit(u_int32_t value);

  void setCSThreshold(u_int32_t value);

  virtual int command(int argc, const char*const* argv);


protected:


  /** 
   * Called by the MAC each time counters are updated.
   *
   * The purpose of this method is to invoke both
   * this->counterEvent() and perform some routine needed by the base
   * class. This method is not supposed
   * to be overloaded.
   *
   * @param id the id of the counter which has just been updated
   */
  void counterHandle(CounterId id);



  virtual MR_PHY_MIB* getPhyMib();
  virtual MR_MAC_MIB* getMacMib();
  //  MR_MAC_MIB* getOldMacMib();


  virtual PhyMode getBasicMode();
  virtual PhyMode getDataMode();

  virtual PER* getPER();

  virtual PeerStats* getPeerStats(u_int32_t src, u_int32_t dst);

  Mac802_11mr *mac_;

  Mac80211EventHandler* next;

  //  MR_MAC_MIB* old_MAC_MIB;
  


};









#endif  /*  MAC80211EVENTHANDLER_H */


