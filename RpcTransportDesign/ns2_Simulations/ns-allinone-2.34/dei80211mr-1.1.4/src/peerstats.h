/* -*-  Mode:C++; -*- */
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



#ifndef PEERSTATS_H
#define PEERSTATS_H

class MR_MAC_MIB;


/**
 * Statistics related to the communications between two 802.11 instances.
 * which is denoted by the pair of mac addresses (src,dest)
 */
class PeerStats
{
public:
  PeerStats(); 
  ~PeerStats();

  virtual void updateSnr(double value)       { snr=value; }
  virtual void updateSnir(double value)      { snir=value; }
  virtual void updateInterf(double value)    { interf=value; }

  virtual double getSnr()    { return snr; }
  virtual double getSnir()   { return snir; }
  virtual double getInterf() { return interf; }

  char* peerstats2string(char* str, int size);

  MR_MAC_MIB* macmib;  /**< holds all the counters for each (src,dest) */


protected:

  /*
   * NOTE: these are last values seen for each (src,dest) 
   * this does not make much sense since values
   * can vary pretty much among subsequent
   * transmissions (especially sinr and interference).
   * These protected member are provided only as the most basic
   * implementation. A better implementation would inherit from
   * PeerStats and overload the virtual public updateValue() and
   * getValue() methods in order to, e.g., return  average values.
   */

  double snr;          /**< Signal to Noise ratio in dB */
  double snir;         /**< Signal to Noise and Interference ratio in dB */
  double interf;       /**< Interference power in W */
};



/**
 * Interface to the database containing PeerStats class instances for each connection.
 * Each implementation of this database must be plugged to MAC
 * instances. Depending on the implementation, a global and omniscient
 * database might be plugged in all MAC instances, or a per-MAC
 * instance of the database can be created.
 * 
 */
class PeerStatsDB : public TclObject 
{
public:

  PeerStatsDB();
  virtual ~PeerStatsDB();				

  /** 
   * Returns the PeerStats data related to the communication between
   * src and dst. Note that either src or dst should refer to the MAC
   * calling this method, in order for per-mac PeerStatsDB
   * implementations to implement efficient memory management. 
   * Particular PeerStatsDB implementations might offer a more relaxed
   * behaviour, but the caller of this method should not rely on it if
   * it's not aware of which PeerStatsDB implementation it's using.
   * 
   * @param src MAC address of the sender
   * @param dst MAC address of the receiver
   * 
   * @return pointer to the PeerStats class containing the required stats.
   */
  virtual PeerStats* getPeerStats(u_int32_t src, u_int32_t dst) = 0;

  int command(int argc, const char*const* argv);


protected:
  int VerboseCounters_;


};



#endif /* PEERSTATS_H */
