/* -*- Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t
              -*- */

/*
 * Copyright (C) 1997 by the University of Southern California
 * $Id: classifier-mpath.cc,v 1.10 2005/08/25 18:58:01 johnh Exp $
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * The copyright of this module includes the following
 * linking-with-specific-other-licenses addition:
 *
 * In addition, as a special exception, the copyright holders of
 * this module give you permission to combine (via static or
 * dynamic linking) this module with free software programs or
 * libraries that are released under the GNU LGPL and with code
 * included in the standard release of ns-2 under the Apache 2.0
 * license or under otherwise-compatible licenses with advertising
 * requirements (or modified versions of such code, with unchanged
 * license).  You may copy and distribute such a system following the
 * terms of the GNU GPL for this module and the licenses of the
 * other code concerned, provided that you include the source code of
 * that other code when and as the GNU GPL requires distribution of
 * source code.
 *
 * Note that people who make modified versions of this module
 * are not obligated to grant this special exception for their
 * modified versions; it is their choice whether to do so.  The GNU
 * General Public License gives permission to release a modified
 * version without this exception; this exception also makes it
 * possible to release a modified version which carries forward this
 * exception.
 *
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/classifier/classifier-mpath.cc,v 1.10 2005/08/25 18:58:01 johnh Exp $ (USC/ISI)";
#endif

#include "classifier.h"
#include "ip.h"

class MultiPathForwarder : public Classifier {
public:
  MultiPathForwarder() : ns_(0), nodeid_(0), nodetype_(0), perflow_(0), checkpathid_(0) {
		bind("nodeid_", &nodeid_);
		bind("nodetype_", &nodetype_);
		bind("perflow_", &perflow_);
		bind("checkpathid_", &checkpathid_);
	}
	virtual int classify(Packet* p) {
      	int cl;
		hdr_ip* h = hdr_ip::access(p);
		// Mohammad: multipath support
		// fprintf(stdout, "perflow_ = %d, rcv packet in classifier\n", perflow_);
		if (perflow_ || checkpathid_)
        {
		  /*if (h->flowid() >= 10000000) {
		  	int fail = ns_;
			do {
			  cl = ns_++;
			  ns_ %= (maxslot_ + 1);
			} while (slot_[cl] == 0 && ns_ != fail);
			return cl;
			}*/

            struct hkey
            {
			    int nodeid;
       			nsaddr_t src, dst;
                int fid;
		    };
		    struct hkey buf_;
		    buf_.nodeid = nodeid_;
		    buf_.src = mshift(h->saddr());
		    buf_.dst = mshift(h->daddr());
		    buf_.fid = h->flowid();
		    /*if (checkpathid_)
                buf_.prio = h->prio();
		    else
                buf_.prio = 0;*/
            char* bufString = (char*) &buf_;
            int length = sizeof(hkey);

            unsigned int ms_ = (unsigned int) HashString(bufString, length);
            if (checkpathid_)
            {
                int pathNum = h->prio();
                int pathDig;
                for (int i = 0; i < nodetype_; i++)
                {
                    pathDig = pathNum % 8;
                    pathNum /= 8;
		        }
                //printf("%d: %d->%d\n", nodetype_, h->prio(), pathDig);
		        ms_ += h->prio(); //pathDig;
		    }
		    ms_ %= (maxslot_ + 1);
		    //printf("nodeid = %d, pri = %d, ms = %d\n", nodeid_, buf_.prio, ms_);
		    int fail = ms_;
		    do {
			    cl = ms_++;
                ms_ %= (maxslot_ + 1);
		    } while (slot_[cl] == 0 && ms_ != fail);
		    //printf("nodeid = %d, pri = %d, cl = %d\n", nodeid_, h->prio(), cl);
		} else
        {
		  //hdr_ip* h = hdr_ip::access(p);
		  //if (h->flowid() == 45) {
		  //cl = h->prio() % (maxslot_ + 1);
		  //}
		  //else {
          int fail = ns_;
          do {
              cl = ns_++;
              ns_ %= (maxslot_ + 1);
          } while (slot_[cl] == 0 && ns_ != fail);
        }


		return cl;
	}
private:
	int ns_;
	// Mohamamd: adding support for perflow multipath
	int nodeid_;
    int nodetype_;
	int perflow_;
	int checkpathid_;

	static unsigned int
	HashString(register const char *bytes,int length)
	{
		register unsigned int result;
		register int i;

		result = 0;
		for (i = 0;  i < length;  i++) {
			result += (result<<3) + *bytes++;
		}
		return result;
	}
};

static class MultiPathClass : public TclClass {
public:
	MultiPathClass() : TclClass("Classifier/MultiPath") {}
	TclObject* create(int, const char*const*) {
		return (new MultiPathForwarder());
	}
} class_multipath;
