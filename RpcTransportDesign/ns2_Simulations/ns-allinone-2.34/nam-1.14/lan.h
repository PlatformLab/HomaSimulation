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
 * @(#) $Header: /usr/src/mash/repository/vint/nam-1/edge.h,v 1.5 1997/10/04 19
:54:47 mjh Exp $ (LBL)
*/

#ifndef nam_lan_h
#define nam_lan_h


#include <math.h>
#include "animation.h"
#include "transform.h"
#include "edge.h"
#include "node.h"
#include "netmodel.h"

class View;
class Transform;
class Packet;
class Monitor;
class NetModel;


class LanLink {
    public:
        LanLink(Edge *e);
	LanLink *next() const {return next_;}
	Edge *edge() const {return edge_;}
	Edge *pair() const {return pair_;}
	void next(LanLink *link) {next_=link;}
	int placed();
    private:
        LanLink *next_;
        Edge *edge_, *pair_;
};

class Lan : public Animation{
public:
  Lan(const char *name, NetModel *nm, double ps, double bw, 
      double delay, double angle);
	~Lan();
	virtual float distance(float x, float y) const;
	void add_link(Edge *e);
	int no_of_nodes() const { return (no_of_nodes_); }
	int num() const { return ln_; }
	void draw(class View *nv, double time);
  //	void draw(class PSView *nv, double time) const;
	void update_bb();
	void size(double size);
	virtual void place(double x, double y);
	Lan *next_;
	Node *virtual_node() {return virtual_node_;}
	inline int marked() const { return (marked_); }
	inline void mark() { marked_ = 1; }
        inline void unmark() { marked_ = 0; }
	double bw() const {return bw_;}
	double delay() const {return delay_;}
	void arrive_packet(Packet *p, Edge *e, double atime);
	void delete_packet(Packet *p);
	double x(Edge *e) const;
	double y(Edge *e) const;
	virtual int classid() const { return ClassLanID; }

	Tcl_HashTable *dropHash_;
	void register_drop(const TraceEvent &);
	void remove_drop(const TraceEvent &);
    private:
	NetModel *nm_;
	VirtualNode *virtual_node_;
	LanLink *links_;
	int no_of_nodes_;
	double x_, y_, size_;
	double ps_, bw_, delay_, angle_; // packet size, bandwidth, delay, angle
	char *name_;
	int ln_;
	int marked_;
	int max_;
};

#endif
