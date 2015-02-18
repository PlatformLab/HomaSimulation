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

#include"peerstatsdb_static.h"

static class StaticPeerStatsDBClass : public TclClass {
public:
	StaticPeerStatsDBClass() : TclClass("PeerStatsDB/Static") {}
	TclObject* create(int, const char*const*) {
		return (new StaticPeerStatsDB);
	}
} class_static_peer_stats_db_class;


PeerStats* StaticPeerStatsDB::getPeerStats(u_int32_t src, u_int32_t dst)
{
  assert(src < num_peers);
  assert(dst < num_peers);
  assert(src>=0);
  assert(dst>=0);
  assert(peerstats);
  
  return (&(peerstats[src][dst]));

}



StaticPeerStatsDB::StaticPeerStatsDB()
  : peerstats(0),
    num_peers(0)
{
  bind("debug_", &debug_);
}


StaticPeerStatsDB::~StaticPeerStatsDB()
{
  deallocate();
}



void StaticPeerStatsDB::allocate()
{
  assert(num_peers>0);
   try
     {
       peerstats = (PeerStats**) new PeerStats*[num_peers];
       for (int i=0; i< num_peers; i++)
	 {
	   peerstats[i] = new PeerStats[num_peers];
	 }
     }
   catch(exception &e)
     {
       std::cerr << "StaticPeerStatsDB::allocate() exception caught" << std::endl;
       abort();
     }
        
}


void StaticPeerStatsDB::deallocate()
{
    for (int i=0; i< num_peers; i++)
      {
	delete[] peerstats[i];
	peerstats[i] = 0;
	
      }

    delete[] peerstats;
    peerstats = 0;
    
}


int StaticPeerStatsDB::command(int argc, const char*const* argv)
{
  Tcl& tcl = Tcl::instance();

  if (debug_)
    cerr << "StaticPeerStatsDB::command(\"" << argv[1] << "\")" << endl;
  
  if (argc == 2) {
    if(strcasecmp(argv[1], "dump") == 0) {
      dump();
      return (TCL_OK);
    }      
  }
  if (argc == 3) {
    if(strcasecmp(argv[1], "numpeers") == 0) {
      deallocate();
      num_peers = atoi(argv[2]);
      allocate();
      if (debug_) 
	cerr << "StaticPeerStatsDB::command   numpeers="
	     << num_peers << endl;
      return (TCL_OK);
    }      
  }

  return PeerStatsDB::command(argc,argv);
}


void  StaticPeerStatsDB::dump()
{
  char str[500];
  
  for (int src=0; src< num_peers; src++)
    for (int dst=0; dst< num_peers; dst++)
      {
	PeerStats* ps = getPeerStats(src,dst);
	assert(ps);

// 	assert(ps->macmib);
// 	ps->macmib->counters2string(str, 500);
// 	printf("%3d --> %3d %6.3f %6.3f %6.3f    %s\n",
// 	       src,
// 	       dst,
// 	       ps->getSnr(),
// 	       ps->getSnir(),
// 	       ps->getInterf(),	       
// 	       str);

	ps->peerstats2string(str, 500);
	printf("%3d <--> %3d   %s\n",src, dst, str);
	
      }
}

