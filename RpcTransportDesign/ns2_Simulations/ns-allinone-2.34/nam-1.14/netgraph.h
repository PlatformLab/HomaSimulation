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
 * @(#) $Header: /cvsroot/nsnam/nam-1/netgraph.h,v 1.5 2006/09/28 06:25:03 tom_henderson Exp $ (LBL)
 */

#ifndef nam_netgraph_h
#define nam_netgraph_h

class Edge;
class Node;
class Route;
class Agent;
struct TraceEvent;
class Queue;
class Animation;
class GraphView;
class Paint;
class Packet;
struct BBox;

#include "trace.h"

#define MAX_GRAPH 2048

#define GR_BW 1
#define GR_LOSS 2

class NetGraph : public TraceHandler {
    public:
	NetGraph();
	virtual ~NetGraph();

	/* TraceHandler hooks */
	void update(double now, Animation* a);
	void update(double);
	void reset(double);
	virtual void handle(const TraceEvent&, double now, int direction);
	virtual int command(int argc, const char *const *argv);
	virtual void BoundingBox(BBox&);
	virtual void render(GraphView* view);
    protected:
	GraphView *views_;
	float graphdata_[MAX_GRAPH];
	float scaleddata_[MAX_GRAPH];
	int minx_, maxx_;
	float miny_, maxy_;
	float mintime_, maxtime_;
	double time_;
	int tix_;  //time index - used to avoid having to redraw the graph
                   //except when it makes a difference
	int ncolors_;
	int *paint_;
};

class EnergyNetGraph : public NetGraph {
    public:
        EnergyNetGraph();
	~EnergyNetGraph();
	virtual int command(int argc, const char *const *argv);
	void handle(const TraceEvent&, double now, int direction);
};

class LinkNetGraph : public NetGraph {
    public:
        LinkNetGraph();
	~LinkNetGraph();
	virtual int command(int argc, const char *const *argv);
	void handle(const TraceEvent&, double now, int direction);
    protected:        
        int src_, dst_, gtype_;
};

class FeatureNetGraph : public NetGraph {
 public:
        FeatureNetGraph();
	~FeatureNetGraph();
	virtual int command(int argc, const char *const *argv);
	void handle(const TraceEvent&, double now, int direction);
	void BoundingBox(BBox&);
 protected:
	int src_;
	char *aname_;
	char *vname_;
	int lastix_;
};
#endif
