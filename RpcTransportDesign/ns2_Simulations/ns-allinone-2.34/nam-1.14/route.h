/*
 * Copyright (c) 1997 University of Southern California.
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
 *      This product includes software developed by the Information Sciences
 *      Institute of the University of Southern California.
 * 4. Neither the name of the University nor of the Institute may be used
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
 * @(#) $Header: /cvsroot/nsnam/nam-1/route.h,v 1.8 2001/04/18 00:14:16 mehringe Exp $ (LBL)
 */

#ifndef nam_route_h
#define nam_route_h

class Queue;
class Edge;
class Agent;
class View;
class MonState;
class Monitor;

#include "animation.h"

/*conceivably there are other options than NEGATIVE/POSITIVE caches, 
  or IIF/OIF*/ 
#define NEG_CACHE 1
#define POS_CACHE 2
#define IIF 4
#define OIF 8

class Route : public Animation {
    public:
	Route(Node *, Edge *, int group, int pktsrc,
	      int negcache, int iif, double timer, double now);
	~Route();
	virtual void place(double x, double y);
	virtual void place(double x, double y, int ctr);
	//virtual int inside(double, float, float) const;
	inline int marked() const { return (mark_); }
	inline void mark(int v) { mark_ = v; }
	inline Route *next() const { return next_; }
	inline Edge *edge() const { return edge_; }
	inline Node *node() const { return node_; }
	inline double angle() const {return angle_;}
	inline void next(Route *r) { next_=r; };
	int matching_route(Edge *e, int group, int pktsrc, int oif) const;
	const char* info() const;
	const char* getname() const;
	void monitor(Monitor *m, double now, char *result, int len);
	MonState *monitor_state();
	virtual void draw(View* nv, double now);
  //virtual void draw(PSView*, double now) const;
	virtual void reset(double now);
	virtual void update(double now);

	Route* next_;
    protected:
	void update_bb();

	Edge *edge_;   /*the edge corresponding to the interface*/
	Node *node_;
	int group_;    /*the multicast group, or -1 for unicast*/
	int pktsrc_;   /*the src for which this is a route*/
	int mode_;     /*OR(POS_CACHE,NEG_CACHE,IIF,OIF, etc)*/
	double timeout_;  /*the timer value til timeout*/
	double curtimeout_;  /*current time remaining in timeout*/
	double timeset_;  /*when the timer was set*/
	int anchor_;
	int mark_;

	float x_[4], y_[4];
	int npts_;
        double angle_;
        Transform matrix_;
};

#endif
