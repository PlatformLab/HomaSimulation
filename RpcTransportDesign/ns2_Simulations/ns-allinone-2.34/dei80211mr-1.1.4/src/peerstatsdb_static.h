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

#ifndef  PEERSTATSDB_STATIC_H
#define  PEERSTATSDB_STATIC_H

#include"mac-802_11mr.h"



/**
 * Implementation of PeerStatsDB using a static bi-dimensional array
 * (i.e., allocated only once at the beginning of the simulation
 * regardless of the items which will be actually used).
 * It provides a very quick lookup but does not optimize memory usage.
 */
class StaticPeerStatsDB : public PeerStatsDB
{
public:
  StaticPeerStatsDB();
  ~StaticPeerStatsDB();
  virtual PeerStats* getPeerStats(u_int32_t src, u_int32_t dst);
  int command(int argc, const char*const* argv);

protected:
  void dump();
  PeerStats** peerstats;

  int num_peers; /**< Number of 802.11 peers in the simulation
		  * Remember that the memory needed by StaticPeerStatsDB
		  * is sizeof(PeerStats)*num_peers^2
		  */

  void allocate();
  void deallocate();
  int debug_;
};




#endif /* PEERSTATSDB_STATIC_H */
