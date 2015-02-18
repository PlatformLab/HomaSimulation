/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1997 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory and the Daedalus
 *	research group at UC Berkeley.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 * Ported from ns-2.29/mac/channel.h by Federico Maguolo, Nicola Baldo and Simone Merlin 
 * (SIGNET lab, University of Padova, Department of Information Engineering)
 *
 */



#ifndef _PAWIRELESSCHANNEL_
#define _PAWIRELESSCHANNEL_

#include "channel.h"

class PAWirelessChannel : public Channel{
	friend class Topography;
public:
	PAWirelessChannel();
	virtual int command(int argc, const char*const* argv);
//         inline double gethighestAntennaZ() { return highestAntennaZ_; }

private:
	void sendUp(Packet* p, Phy *txif);
	double get_pdelay(Node* tnode, Node* rnode);
	
	/* For list-keeper, channel keeps list of mobilenodes 
	   listening on to it */
	int numNodes_;
	MobileNode *xListHead_;
	bool sorted_;
	void addNodeToList(MobileNode *mn);
	void removeNodeFromList(MobileNode *mn);
	void sortLists(void);
	void updateNodesList(class MobileNode *mn, double oldX);
	MobileNode **getAffectedNodes(MobileNode *mn, double radius, int *numAffectedNodes);
	
protected:

	// CS threshold is no more used to determined affected nodes
	// this is because nodes well below the CS threshold might still provide
	// a non-negligible contribution to interference
//      static double distCST_;        



	/**
	 * Nodes beyond this distance are not considered for
	 * interference calculations. This is useful to reduce
	 * computational load in very large multihop topologies 
	 * It should however be used with caution in order to preserve 
	 * the correctness of interference calculations.
	 * Also remember that the distance metric used for this
	 * purpose is the Manhattan distance, not the Euclidean
	 * distance. 
	 */
	double distInterference_; 



	// Not needed any more since distInterference_ is explicitly
	// set via tcl in meters. 
//      static double highestAntennaZ_;
//      void calcHighestAntennaZ(Phy *tifp);

};

#endif
