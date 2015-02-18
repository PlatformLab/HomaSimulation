/*
 * Copyright (c) 1991,1993 Regents of the University of California.
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
 *	Engineering Group at Lawrence Berkeley Laboratory.
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
 * @(#) $Header: /cvsroot/nsnam/nam-1/trace.h,v 1.38 2003/01/28 03:22:38 buchheim Exp $ (LBL)
 */

#ifndef nam_trace_h
#define nam_trace_h

#include "tclcl.h"
#include "animator.h"
#include "parser.h"
class NamStream;
//class ParseTable;

#define TRACE_LINE_MAXLEN 256

/*
 * 'packet' events (hop, enqueue, dequeue & drop) all have the same
 * format:  the src & dst node and a description of the packet.
 */

#define PTYPELEN 16
#define CONVLEN 32

#define FORWARDS 1
#define BACKWARDS -1

#define TIME_EOF -1
#define TIME_BOF -2

struct PacketAttr {
	int size;
	int attr;
  int id;
	int energy ;
	int esrc ;
	int edst ;
	double wBcastDuration;
	double wBcastRadius;
	char type[PTYPELEN];
	char wtype[PTYPELEN]; //AGT,RTR or MAC
	char convid[CONVLEN];
};

struct PacketEvent {
	int src;
	int dst;
	PacketAttr pkt;
	PacketAttr namgraph_flags; // for ignoring namgraph flags
};

struct VarEvent {
	/* Var event stuff */
	int str;
	//char * str;
};

/*XXX don't really want these fixed size*/
#define MAXNAME 20
#define MAXVALUE 256

struct AgentAttr {
  char name[MAXNAME];
  int expired;
};
  
struct AgentEvent {
  /*Agent event stuff*/
  int src;
  /*dst is only set if it's an interface agent*/
  int dst;
  AgentAttr agent;
};

struct FeatureAttr {
  char name[MAXNAME];
  char value[MAXVALUE];
  char oldvalue[MAXVALUE];
  char agent[MAXNAME];
  int expired;
  char type;
};

struct FeatureEvent {
  int src;
  /*XXX not sure we need dst here*/
  int dst;
  FeatureAttr feature;
};
  
struct LinkAttr {
  double rate;              // Link Bandwith (Rate)
  double delay;             // Link Delay
  double length;            // length
  double angle;             // orientation

  char state[MAXVALUE];
  char color[MAXNAME];
  char oldColor[MAXNAME];
  char dlabel[MAXNAME];     // label beneath a link
  char odlabel[MAXNAME];    // old dlabel
  char direction[MAXNAME];  // direction of the label 
  char odirection[MAXNAME]; 
  char dcolor[MAXNAME];      // label color
  char odcolor[MAXNAME];
};

struct LinkEvent {
  int src;
  int dst;
  LinkAttr link;
};

struct LanLinkEvent {
  int src;
  int dst;
  double angle;
};

struct LayoutLanEvent {
	char name[MAXVALUE];
  double rate;
  double delay;
  double angle;
};

struct NodeMarkAttr {
	char name[MAXVALUE];
	char shape[MAXNAME];
	char color[MAXNAME];
	int expired;
};

struct NodeMarkEvent {
	int src;
	NodeMarkAttr mark;
};

struct NodeAttr {
int addr;
  char state[MAXNAME];
  char dlabel[MAXNAME];      // label beneath a node
  char odlabel[MAXNAME];     // old dlabel
  char color[MAXNAME];
  char oldColor[MAXNAME];
  char direction[MAXNAME];   // direction of the label 
  char odirection[MAXNAME];
  char lcolor[MAXNAME];      // inside label color
  char olcolor[MAXNAME];
  char dcolor[MAXNAME];      // label color
  char odcolor[MAXNAME];
};

struct NodeEvent {
  int src;
  /*dst is only set if it's an interface event*/
  int dst;
  NodeAttr node;
  NodeMarkAttr mark;  // Added for dynamic nodes
  double x;
  double y;
  double z;  // Not fully implemented
  double x_vel_;
  double y_vel_;
  double stoptime;
  double size;
  int wireless;
};

const int GROUP_EVENT_JOIN = 1;
const int GROUP_EVENT_LEAVE = 2;

struct GroupAttr {
	char name[MAXNAME];
	int flag; 	/* 1: join, 2: leave, 3: create new group */
	int mbr;
};

struct GroupEvent {
	int src;	/* group address */
	GroupAttr grp;
};

struct RouteAttr {
  int expired;
  int neg;
  double timeout;
  int pktsrc;
  int group;
  char mode[MAXVALUE];
};

struct RouteEvent {
  int src;
  int dst;
  RouteAttr route;
};

struct HierarchicalAddressEvent {
  int hierarchy;
  int portshift;
  char portmask[MAXNAME];
  int multicast_shift;
  int multicast_mask;
  
  int nodeshift;
  int nodemask;
};

struct ColorEvent {
  int id;
  char color[MAXNAME];
};

struct QueueEvent {
  int src;
  int dst;
  double angle;
};

struct WirelessEvent {
  int x;
  int y;
};

struct TrafficSourceEvent {
  int id;
  int agent_id; 
};

struct TraceEvent {
    double time;		/* event time */
    long offset;		/* XXX trace file offset */
    int lineno;	  	/* XXX trace file line no. */
    int tt;			    /* type: h,+,-,d */
    union {
      PacketEvent pe;
      VarEvent ve;
      AgentEvent ae;
      FeatureEvent fe;
      LinkEvent le;
      LanLinkEvent lle;
      LayoutLanEvent layoutle;
      NodeEvent ne;
      RouteEvent re;
      NodeMarkEvent me;
      GroupEvent ge;
      HierarchicalAddressEvent hae;
      ColorEvent ce;
      QueueEvent qe;
      WirelessEvent we;
    };

    char version[MAXNAME];
    int dummy;  // Used for variables that are discarded
    char dummy_str[MAXNAME];  // Used for variables that are discarded
    bool valid;
    char image[TRACE_LINE_MAXLEN];
};


class TraceHandler : public TclObject {
public:
  TraceHandler() : nam_(0) {}
  TraceHandler(const char *animator) {
    nam_ = (NetworkAnimator *)TclObject::lookup(animator);
  }
  virtual void update(double) = 0;
  virtual void reset(double) = 0;
  virtual void handle(const TraceEvent&, double now, int direction) = 0;
  inline NetworkAnimator* nam() { return nam_; }
protected:
  NetworkAnimator *nam_;
};


struct TraceHandlerList {
	TraceHandler* th;
	TraceHandlerList* next;
};


class Trace : public TclObject {
 public:
	Trace(const char * fname, const char * animator);
	~Trace();
	int command(int argc, const char*const* argv);
	int ReadEvent(double, TraceEvent&);
	void scan();
	void rewind(long);
	int nextLine();
	double nextTime() { return (pending_.time); }
	double Maxtime() { return (maxtime_); }
	double Mintime() { return (mintime_); }
	int valid();
	NetworkAnimator* nam() { return nam_; }

	Trace* next_;
 private:
	void addHandler(TraceHandler*);
	void settime(double now, int timeSliderClicked);
	void findLastLine();

	TraceHandlerList* handlers_;
	int lineno_;
	double maxtime_;
	double mintime_;
	double now_;
	NamStream *nam_stream_;
	int direction_;  //  1 = Forwards, -1 = Backwards
	double last_event_time_; // Used for reporting trace syntax timing errors
	TraceEvent pending_;
	char fileName_[256];
	NetworkAnimator * nam_;

	int skipahead_mode_; 
	int count_;
  
	ParseTable * parse_table_;
};

#endif

