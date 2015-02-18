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
 *      This product includes software developed by the Computer Systems
 *      Engineering Group at Lawrence Berkeley Laboratory.
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
 * @(#) $Header: /cvsroot/nsnam/nam-1/netmodel.cc,v 1.113 2006/11/19 00:12:29 tom_henderson Exp $ (LBL)
 */

#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <tcl.h>
#include <tclcl.h>

#include "config.h"
#include "netview.h"
#include "psview.h"
#include "testview.h"
#include "animation.h"
#include "group.h"
#include "tag.h"
#include "queue.h"
#include "drop.h"
#include "packet.h"
#include "edge.h"
#include "lan.h"
#include "node.h"
#include "agent.h"
#include "feature.h"
#include "route.h"
#include "netmodel.h"
#include "monitor.h"
#include "trace.h"
#include "paint.h"
#include "sincos.h"
#include "state.h"
#include "editview.h"
#include "address.h"
#include "animator.h"

#include <float.h>

extern int lineno;
// not used
//static int next_pat;

class NetworkModelClass : public TclClass {
public:
	NetworkModelClass() : TclClass("NetworkModel") {}
	TclObject* create(int argc, const char*const* argv) {
		if (argc < 5) 
			return 0;
		return (new NetModel(argv[4]));
	}
} networkmodel_class;

//----------------------------------------------------------------------
//----------------------------------------------------------------------
NetModel::NetModel(const char *animator) :
	TraceHandler(animator),
	drawables_(0),
	animations_(0),
	queues_(0),
	views_(0),
	nodes_(0),
	lans_(0), 
	node_sizefac_(NODE_EDGE_RATIO),
	mon_count_(0),
	monitors_(NULL),
	wireless_(0),
	resetf_(0),
	selectedSrc_(-1),
	selectedDst_(-1),
	selectedFid_(-1),
	hideSrc_(-1),
	hideDst_(-1),
	hideFid_(-1),
	colorSrc_(-1),
	colorDst_(-1),
	colorFid_(-1),
	showData_(1), 
	showRouting_(1),
	showMac_(1),
	selectedColor_(-1),
	nGroup_(0), 
	nTag_(0),
	parsetable_(&traceevent_) {

	int i;
	for (i = 0; i < EDGE_HASH_SIZE; ++i) {
		hashtab_[i] = 0;
	}

	for (i = 0; i < PTYPELEN; ++i) {
		selectedTraffic_[i] = '\0' ;
		colorTraffic_[i] = '\0' ;
		hideTraffic_[i] = '\0' ;
	}

	// Default node size is 10.0 so a default packet will be 25% of that (2.5)
	// This value is modified whenever a node is added.  It will be based on 
	// the running average of the size of the last 5 nodes.  Look at 
	// NetModel::addNode(const TraceEvent &e) for more details
	packet_size_ = 2.5;
  
	/*XXX*/
	nymin_ = 1e6;
	nymax_ = -1e6;

	Paint *paint = Paint::instance();
	int p = paint->thin();
	// Initially 256 colors. Can be extended later.
	// See handling of otcl binding "color"
	nclass_ = 256;
	paintMask_ = 0xff;
	paint_ = new int[nclass_];
	oldpaint_ = new int[nclass_];
	for (i = 0; i < nclass_; ++i) {
		paint_[i] = p;
		oldpaint_[i] = p;
  }

	addrHash_ = new Tcl_HashTable;
	Tcl_InitHashTable(addrHash_, TCL_ONE_WORD_KEYS);
	grpHash_ = new Tcl_HashTable;
	Tcl_InitHashTable(grpHash_, TCL_ONE_WORD_KEYS);
	tagHash_ = new Tcl_HashTable;
	Tcl_InitHashTable(tagHash_, TCL_STRING_KEYS);

	objnameHash_ = new Tcl_HashTable;
	Tcl_InitHashTable(objnameHash_, TCL_STRING_KEYS);
	registerObjName("ALL", ClassAllID);
	registerObjName("ANIMATION", ClassAnimationID);
	registerObjName("NODE", ClassNodeID);
	registerObjName("PACKET", ClassPacketID);
	registerObjName("EDGE", ClassEdgeID);
	registerObjName("QUEUEITEM", ClassQueueItemID);
	registerObjName("LAN", ClassLanID);
	registerObjName("TAG", ClassTagID);
	registerObjName("AGENT", ClassAgentID);

	bind("bcast_duration_", &bcast_duration_);
	bind("bcast_radius_", &bcast_radius_);
}

NetModel::~NetModel()
{
	// We should delete everything here, if we want deletable netmodel...
	delete paint_;
	Animation *a, *n;
	for (a = animations_; a != 0; a = n) {
		n = a->next();
		delete a;
	}
	for (a = drawables_; a != 0; a = n) {
		n = a->next();
		delete a;
	}

	Tcl_DeleteHashTable(grpHash_);
	delete grpHash_;
	Tcl_DeleteHashTable(tagHash_);
	delete tagHash_;
	Tcl_DeleteHashTable(objnameHash_);
	delete objnameHash_;
}

void NetModel::update(double now)
{
	Animation *a, *n;
	for (a = animations_; a != 0; a = n) {
		n = a->next();
		a->update(now);
	}

	/*
	 * Draw all animations and drawables on display to reflect
	 * current time.
	 */
	now_ = now;
	for (View* p = views_; p != 0; p = p->next_) {
		//  Calls View::draw() which calls NetView::render()
		//  which calls NetModel::render(View*)
		p->draw();
	}
}

void NetModel::update(double now, Animation* a) {
	a->update(now);
	for (View* p = views_; p != 0; p = p->next_)
		a->draw(p, now);
}

//----------------------------------------------------------------------
// void
// NetModel::reset(double now)
//   - Reset all animations and queues to time 'now'.
//----------------------------------------------------------------------
void
NetModel::reset(double now) {
	Animation* a;
	for (a = animations_; a != 0; a = a->next())
		a->reset(now);
	for (a = drawables_; a != 0; a = a->next())
		a->reset(now);
	for (Queue* q = queues_; q != 0; q = q->next_)
		q->reset(now);
}

//----------------------------------------------------------------------
// void
// NetModel::render(View * view)
//   - Draw this NetModel's drawables, animations, and monitors.
//     (tags, nodes, edges, packets, queues, etc.)
//----------------------------------------------------------------------
void
NetModel::render(View* view) {
	Animation *a;
	Monitor *m;

	for (a = drawables_; a != 0; a = a->next()) 
	  a->draw(view, now_);

	for (a = animations_; a != 0; a = a->next())
	  a->draw(view, now_);

	for ( m = monitors_; m != NULL; m = m->next())
	  m->draw_monitor(view, nymin_, nymax_);
}

void NetModel::render(PSView* view) {
	Animation *a;
	for (a = drawables_; a != 0; a = a->next())
	  a->draw(view, now_);
	for (a = animations_; a != 0; a = a->next())
	  a->draw(view, now_);
}

void NetModel::render(TestView* view) {
	Animation *a;
	for (a = drawables_; a != 0; a = a->next())
	  a->draw(view, now_);
	for (a = animations_; a != 0; a = a->next())
	  a->draw(view, now_);
}

//----------------------------------------------------------------------
// NetModel::EdgeHashNode *
// NetModel::lookupEdgeHashNode(int src, int dst) const
//   - Return a pointer to the edge between 'src' and 'dst'. 
//----------------------------------------------------------------------
NetModel::EdgeHashNode *
NetModel::lookupEdgeHashNode(int source, int destination) const
{
	EdgeHashNode* h;
	for (h = hashtab_[ehash(source, destination)]; h != 0; h = h->next)
		if (h->src == source && h->dst == destination)
			break;
	return (h);
}

int NetModel::addAddress(int id, int addr) const
{
	int newEntry = 1;
	Tcl_HashEntry *he = 
		Tcl_CreateHashEntry(addrHash_, (const char *)addr, &newEntry);
	if (he == NULL)
		return -1;
	if (newEntry) {
		Tcl_SetHashValue(he, (ClientData)id);
	}
	return 0;
}

int NetModel::addr2id(int addr) const 
{
	Tcl_HashEntry *he = Tcl_FindHashEntry(addrHash_, (const char *)addr);
	if (he == NULL)
		return -1;
	return *Tcl_GetHashValue(he);
}

//----------------------------------------------------------------------
//  Adds an edge to a hash table?
//----------------------------------------------------------------------
void NetModel::enterEdge(Edge* e) {
	int src = e->src();
	int dst = e->dst();
	EdgeHashNode *h = lookupEdgeHashNode(src, dst);
	if (h != 0) {
		/* XXX */
		fprintf(stderr, "nam: duplicate edge (%d,%d)\n", src, dst);
		//exit(1);
		return;
	}
	h = new EdgeHashNode;
	h->src = src;
	h->dst = dst;
	h->queue = 0;
	h->edge = e;
	int k = ehash(src, dst);
	h->next = hashtab_[k];
	hashtab_[k] = h;
}

//----------------------------------------------------------------------
// void
// NetModel::removeEdge(Edge* e)
//   - Remove an edge from the network model and delete it
//----------------------------------------------------------------------
void
NetModel::removeEdge(Edge* e) {
  int k;
  int src = e->src();
  int dst = e->dst();
  EdgeHashNode * h = lookupEdgeHashNode(src, dst);
  EdgeHashNode * f, * g;  

  if (h == 0) {
    fprintf(stderr, "nam: trying to delete nonesisting edge (%d,%d)\n", src, dst);
    exit(1);
  }

  //XXX do we need to process queue ? leave it to the future 10/01/98
  k = ehash(src, dst);

  for (f = hashtab_[k]; f != 0; f = f->next) {
    if (h->src == f->src && h->dst == f->dst) {
      if (f == hashtab_[k]) {
        hashtab_[k] = f->next;
        break;
      } else {
        g->next = f->next;      
        break;
      } 
    }
    g = f;
  }

  
  
  delete h;
}

//----------------------------------------------------------------------
// void 
// NetModel::BoundingBox(BBox& bb) {
// XXX Make this cheaper (i.e. cache it)
//----------------------------------------------------------------------
void NetModel::BoundingBox(BBox& bb) {
	/* ANSI C limits, from float.h */
	bb.xmin = bb.ymin = FLT_MAX;
	bb.xmax = bb.ymax = -FLT_MAX;

	for (Animation* a = drawables_; a != 0; a = a->next()) 
		a->merge(bb);
}

/*
 Animation* NetModel::inside(double now, float px, float py) const
 {
 	for (Animation* a = animations_; a != 0; a = a->next())
 		if (a->inside(now, px, py))
 			return (a);
 	for (Animation* d = drawables_; d != 0; d = d->next())
 		if (d->inside(now, px, py))
 			return (d);

 	return (0);
 }
*/
// Used exclusively for start_info() in nam.tcl. It ignores all tag objects
// and therefore should *not* be used for editing.

Animation* NetModel::inside(float px, float py) const
{

	for (Animation* a = animations_; a != 0; a = a->next()) {
	     if (a->type() == BPACKET) {
	        BPacket* b = (BPacket* ) a ;
		if ((b->inside(px, py)) && (a->classid() != ClassTagID))
			return (a);
	     } 
	     else  {
		if ((a->inside(px, py)) && (a->classid() != ClassTagID))
			return (a);
	     }
	}
	for (Animation* d = drawables_; d != 0; d = d->next()) {
		if ((d->inside(px, py) && (d->classid() != ClassTagID)))
			return (d);
	}

	return (0);
}

int NetModel::add_monitor(Animation *a)
{
  Monitor *m = new Monitor(mon_count_, a, node_size_);
  m->next(monitors_);
  monitors_=m;
  return mon_count_++;
}

int NetModel::monitor(double now, int monitor, char *result, int len)
{
  /*XXX should get rid of this search*/
  for(Monitor *m=monitors_; m!=NULL; m=m->next()) {
    if (m->monitor_number()==monitor) {
      m->update(now, result, len);
      if (strlen(result)==0)
	delete_monitor(m);
      return 0;
    }
  }
  result[0]='\0';
  return -1;
}

//----------------------------------------------------------------------
// void NetModel::check_monitors(Animation *a)
//   - A new animation just got created.  Check to see if we should 
//     already have a monitor on it.
//----------------------------------------------------------------------
void NetModel::check_monitors(Animation *a) {
	MonState * ms;
	Monitor * m;
	// Returns a "newed" MonState
	ms = a->monitor_state();

	if (ms == NULL) {
		return;
	}

	for(m = monitors_; m != NULL; m = m->next()) {
		if ((m->mon_state_ != NULL) &&
				(m->mon_state_->type = ms->type)) {

			switch (ms->type) {
				case MON_PACKET:
					if (m->mon_state_->pkt.id == ms->pkt.id) {
						m->anim(a);
						delete ms;
					  return;
					}
					break;
				case MON_ROUTE: 
					if ((m->mon_state_->route.src == ms->route.src)&&
							(m->mon_state_->route.group == ms->route.group)) {
						m->anim(a);
						delete ms;
						return;
					}
					break;
			}
		}
	}

//  What happens to ms after the end of this function?  Maybe the memory leak?
//	fprintf(stderr, "Reached outside of check_monitors\n");
}

void NetModel::delete_monitor(int monitor)
{
  /*this version of delete_monitor is called from the GUI*/
  /*given a monitor, remove it from the model's monitor list*/
  Monitor *tm1, *tm2;
  if (monitors_==NULL)
    return;
  tm1=monitors_;
  tm2=monitors_;
  while ((tm1!=NULL)&&(tm1->monitor_number()!=monitor))
    {
      tm2=tm1;
      tm1=tm1->next();
    }
  if (tm1!=NULL)
    {
      tm2->next(tm1->next());
      if (tm1==monitors_)
	monitors_=tm1->next();
      if (tm1->anim()!=NULL)
	tm1->anim()->remove_monitor();
      delete tm1;
    }
}

void NetModel::delete_monitor(Monitor *m)
{
  /*given a monitor, remove it from a node's agent list*/
  Monitor *tm1, *tm2;
  if (monitors_==NULL)
    return;
  tm1=monitors_;
  tm2=monitors_;
  while ((tm1!=m)&&(tm1!=NULL))
    {
      tm2=tm1;
      tm1=tm1->next();
    }
  if (tm1!=NULL)
    {
      tm2->next(tm1->next());
      if (tm1==monitors_)
	monitors_=tm1->next();
      tm1->anim()->remove_monitor();
      delete tm1;
    }
}

void NetModel::delete_monitor(Animation *a)
{
  /*given a monitor, remove it from a node's agent list*/
  /*this version gets called when an animation deletes itself*/
  /*XXX animations sometimes get deleted when the packet changed link*/
  Monitor *tm1, *tm2;
  if (monitors_==NULL)
    return;
  tm1=monitors_;
  tm2=monitors_;
  while ((tm1!=NULL)&&(tm1->anim()!=a))
    {
      tm2=tm1;
      tm1=tm1->next();
    }
  if (tm1!=NULL)
    {
      tm2->next(tm1->next());
      if (tm1==monitors_)
	monitors_=tm1->next();
      delete tm1;
    }
}

void NetModel::saveState(double tim)
{
	State* state = State::instance();
	StateInfo min;
	min.time = 10e6;
	min.offset = 0;
	Animation *a, *n;
	StateInfo si;

	/*
	 * Update the animation list first to remove any unnecessary
	 * objects in the list.
	 */
	for (a = animations_; a != 0; a = n) {
		n = a->next();
		a->update(tim);
	}
	for (a = animations_; a != 0; a = n) {
		n = a->next();
		si = a->stateInfo();
		if (si.time < min.time)
			min = si;
	}
	if (min.offset)
		state->set(tim, min);
}

//---------------------------------------------------------------------
// void 
// NetModel::handle(const TraceEvent& e, double now, int direction)
//   - Trace event handler.
//---------------------------------------------------------------------
void 
NetModel::handle(const TraceEvent& e, double now, int direction) {
  QueueItem *q;
  EdgeHashNode *ehn, *ehnrev;
  Edge* ep;
  Node *n;
  //Packet *p;
  Route *r;
  Agent *a;
  Feature *f;
  float x, y;
  int pno;
  double txtime;
  bool do_relayout = false;

  if (e.time > State::instance()->next())
    saveState(e.time);

  switch (e.tt) {
    case 'T':
      // Dummy event no-op used for realtime time synchronization
      break;
              
    case 'v': {  
      // 'variable' -- just execute it as a tcl command 
      if (nam_ == 0) {
        fprintf(stderr, "Couldn't evaluate %s without animator\n",
          e.image);
        break;
      }
      const char *p = e.image + e.ve.str;
      char *q = new char[strlen(nam_->name()) + strlen(p) + 2];
      sprintf(q, "%s %s", nam_->name(), p);
      Tcl::instance().eval(q);
      delete []q;
      break;
    }
    
    case 'h':
      // traffic filter
      if (wireless_) {
        if (strcmp(e.pe.pkt.wtype,"AGT") == 0 ) return ; 
        if (!showData_) { // filter out data packet
           if ((strcmp(e.pe.pkt.wtype,"RTR") == 0 ) ||
               (strcmp(e.pe.pkt.wtype,"MAC") == 0 )) {
              if (((strcmp(e.pe.pkt.type,"cbr") == 0) ||
                   (strcmp(e.pe.pkt.type,"tcp") == 0) ||
                   (strcmp(e.pe.pkt.type,"ack") == 0))) 
              return ;
           }
        }
        if (!showRouting_){ // filter out routing packet
           if (strcmp(e.pe.pkt.wtype,"RTR") == 0 ) {
              if (!((strcmp(e.pe.pkt.type,"cbr") == 0)||
                   (strcmp(e.pe.pkt.type,"tcp") == 0)   ||
                   (strcmp(e.pe.pkt.type,"ack") == 0))) 
              return ;
           }
        }
        if (!showMac_){ // filter out routing packet
           if (strcmp(e.pe.pkt.wtype,"MAC") == 0 ) {
              if (!((strcmp(e.pe.pkt.type,"cbr") == 0)||
                   (strcmp(e.pe.pkt.type,"tcp") == 0)   ||
                   (strcmp(e.pe.pkt.type,"ack") == 0))) 
              return ;
           }
        }
      }

      // show only packet with same chracteristics as selected packet
      if (selectedSrc_ != -1) { //filter being trigger
        if (e.pe.pkt.esrc != selectedSrc_ ) return ;
      }
      if (selectedDst_ != -1) { 
        if (e.pe.pkt.edst != selectedDst_ ) return ;
      }
      if (selectedFid_ != -1) { 
        if (atoi(e.pe.pkt.convid) != selectedFid_ ) return ;
      }
      if (selectedTraffic_[0] != '\0') { 
        if (strcmp(e.pe.pkt.type,selectedTraffic_) != 0 ) return ;
      }

      // hide packet with same chracteristics as selected packet
      if (hideSrc_ != -1) { // filter being trigger
        if (e.pe.pkt.esrc == hideSrc_ ) return ;
      }
      if (hideDst_ != -1) { 
        if (e.pe.pkt.edst == hideDst_ ) return ;
      }
      if (hideFid_ != -1) { 
        if (atoi(e.pe.pkt.convid) == hideFid_ ) return ;
      }
      if (hideTraffic_[0] != '\0') { 
        if (strcmp(e.pe.pkt.type,hideTraffic_) == 0 ) return ;
      }

      //change the packet color on the fly
      if (selectedColor_ < 0 ) {
        Paint *paint = Paint::instance();
        selectedColor_ = paint->lookup("black",1);
      }
      if (colorSrc_ != -1) { 
        if (e.pe.pkt.esrc == colorSrc_ ) 
          paint_[atoi(e.pe.pkt.convid)] = selectedColor_  ;
      }
      if (colorDst_ != -1) { 
        if (e.pe.pkt.edst == colorDst_ ) 
          paint_[atoi(e.pe.pkt.convid)] = selectedColor_  ;
      }
      if (colorFid_ != -1) { 
        if (atoi(e.pe.pkt.convid) == colorFid_ ) 
          paint_[atoi(e.pe.pkt.convid)] = selectedColor_  ;
      }
      if (colorTraffic_[0] != '\0') { 
        if (strcmp(e.pe.pkt.type,colorTraffic_) == 0 ) 
            paint_[atoi(e.pe.pkt.convid)] = selectedColor_  ;
      }

      if (resetf_) 
         paint_[atoi(e.pe.pkt.convid)] = oldpaint_[atoi(e.pe.pkt.convid)] ;

      if (direction==BACKWARDS)
        break;

      if (e.pe.dst == -1) { //broadcasting
        //a quick hack to give fixed transmission + delay time for
        //broadcasting packet
        if (e.time + BPACKET_TXT_TIME < now)
          break ;

        n = lookupNode(e.pe.src);
        BPacket * p = new BPacket(n->x(), n->y(),e.pe.pkt,
                                  e.time,e.offset,direction,
				  e.pe.pkt.wBcastDuration
				  	? e.pe.pkt.wBcastDuration
					: bcast_duration_,
				  e.pe.pkt.wBcastRadius
				  	? e.pe.pkt.wBcastRadius
					: bcast_radius_);
        p->insert(&animations_);
        p->paint(paint_[e.pe.pkt.attr & paintMask_]);
        check_monitors(p);
        break;
      }

      ehn = lookupEdgeHashNode(e.pe.src, e.pe.dst);
      if (ehn == 0 || (ep = ehn->edge) == 0)
        return;

      /*
       * If the current packet will arrive at its destination
       * at a later time, insert the arrival into the queue
       * of animations and set its paint id (id for X graphics
       * context.
       */
      txtime = ep->txtime(e.pe.pkt.size);
      if (e.time + txtime + ep->delay() >= now) {
        /* XXX */
        Packet *p = new Packet(ep, e.pe.pkt, e.time,
                               txtime, e.offset);
        p->insert(&animations_);
        p->paint(paint_[e.pe.pkt.attr & paintMask_]);
        check_monitors(p);

       // fprintf(stderr, "packet %d sent from %d towards %d\n",
       //         e.pe.pkt.id, e.pe.src, e.pe.dst);
      }
      break;

    case 'a':
      n = lookupNode(e.ae.src);
      if (n == 0)
        return;
      a = n->find_agent((char *) e.ae.agent.name);
      if ((direction==FORWARDS)&&(e.ae.agent.expired==0)) {
        if (a == NULL) {
          // will only create an agent if there isn't one already 
          // there with the same name
          a = new BoxAgent(e.ae.agent.name, n->size());
          a->Animation::insert(&animations_);
          n->add_agent(a);
          placeAgent(a, n);
        }
      } else {
        if (a != NULL) {
          n->delete_agent(a);
          delete a;
        }
      }
      break;

    case 'f':
      // We don't need any redraw for this, because it's always 
      // displayed in monitors in a separate pane
      n = lookupNode(e.fe.src);
      if (n == 0)
        return;
      a = n->find_agent((char *)e.fe.feature.agent);
      if (a == 0)
        return;
      f = a->find_feature((char *)e.fe.feature.name);
      if (f == 0) {
        switch (e.fe.feature.type) {
        case 'v':
          f = new VariableFeature(a, e.fe.feature.name);
          break;
        case 'l':
          f = new ListFeature(a, e.fe.feature.name);
          break;
        case 's':
        case 'u':
        case 'd':
          f = new TimerFeature(a, e.fe.feature.name);
          break;
        }
        a->add_feature(f);
      }

      if (((direction==FORWARDS) && (e.fe.feature.expired == 0)) ||
          ((direction==BACKWARDS) && (strlen(e.fe.feature.oldvalue) > 0))) {
        char *value;
        double time_set;
        if (direction==FORWARDS) {
          value=(char *)e.fe.feature.value;
          time_set=now;
        } else {
          value=(char *)e.fe.feature.oldvalue;
          /*XXX need an extra value here*/
          time_set=0.0;
        }
        switch (e.fe.feature.type) {
          case 'v':
          case 'l':
            f->set_feature(value);
            break;
          case 's':
            f->set_feature(atof(value), TIMER_STOPPED, time_set);
            break;
          case 'u':
            f->set_feature(atof(value), TIMER_UP, time_set);
            break;
          case 'd':
            f->set_feature(atof(value), TIMER_DOWN, time_set);
            break;
        }
      } else {
        a->delete_feature(f);
        delete f;
      }
      break;

    case 'r':
      if (direction == FORWARDS)
        break;

      if (e.pe.dst == -1) { //broadcasting
        //a quick hack to give fixed transmission + delay time for
        //broadcasting packet
        if (e.time - BPACKET_TXT_TIME > now)
          break ;

        n = lookupNode(e.pe.src);
        BPacket * p = new BPacket(n->x(), n->y(),e.pe.pkt,
                                  e.time,e.offset,direction,
				  e.pe.pkt.wBcastDuration
				  	? e.pe.pkt.wBcastDuration
					: bcast_duration_,
				  e.pe.pkt.wBcastRadius
				  	? e.pe.pkt.wBcastRadius
					: bcast_radius_);
        p->insert(&animations_);
        p->paint(paint_[e.pe.pkt.attr & paintMask_]);
        check_monitors(p);
        break;
      }

      ehn = lookupEdgeHashNode(e.pe.src, e.pe.dst);
      if (ehn == 0 || (ep = ehn->edge) == 0)
        return;

      /*
       * If the current packet will arrive at its destination
       * at a later time, insert the arrival into the queue
       * of animations and set its paint id (id for X graphics
       * context.
       */
      txtime = ep->txtime(e.pe.pkt.size);
      if (e.time - (txtime + ep->delay()) <= now) {
        /* XXX */
        Packet *p = new Packet(ep, e.pe.pkt,
                               e.time-(ep->delay() + txtime),
                               txtime, e.offset);
        p->insert(&animations_);
        p->paint(paint_[e.pe.pkt.attr & paintMask_]);
        check_monitors(p);
      }
      break;

    case '+':
      ehn = lookupEdgeHashNode(e.pe.src, e.pe.dst);
      if (direction == FORWARDS) {
        if (ehn != 0 && ehn->queue != 0) {
          QueueItem *qi = new QueueItem(e.pe.pkt, e.time,
                                        e.offset);
          qi->paint(paint_[e.pe.pkt.attr & paintMask_]);
          ehn->queue->enque(qi,QUEUE_TAIL);
          qi->insert(&animations_);
          check_monitors(qi);
        }
      } else {
        if (ehn != 0 && ehn->queue != 0) {
          q = ehn->queue->remove(e.pe.pkt);
          delete q;
        }
      }
      break;

    case '-':
      ehn = lookupEdgeHashNode(e.pe.src, e.pe.dst);
      if (direction == FORWARDS) {
        if (ehn != 0 && ehn->queue != 0) {
          q = ehn->queue->remove(e.pe.pkt);
          delete q;
        }
      } else {
        if (ehn != 0 && ehn->queue != 0) {
          QueueItem *qi = new QueueItem(e.pe.pkt, e.time,
                                        e.offset);
          qi->paint(paint_[e.pe.pkt.attr & paintMask_]);
          ehn->queue->enque(qi,QUEUE_HEAD);
          qi->insert(&animations_);
          check_monitors(qi);
        }
      }
      break;

    case 'E': {
      // Nothing for now
      Group *grp = lookupGroup(e.pe.dst);
      if (grp == NULL) {
        fprintf(stderr, "Packet destination group %d not found\n",
          e.pe.dst);
        return;
      }
      int *mbr = new int[grp->size()];
      grp->get_members(mbr);
      for (int i = 0; i < grp->size(); i++) {
        QueueItem *qi = new QueueItem(e.pe.pkt, e.time,
              e.offset);
        qi->paint(paint_[e.pe.pkt.attr & paintMask_]);
        n = lookupNode(mbr[i]);
        if (n == 0) {
          fprintf(stderr, 
            "Group member %d not found\n",
            mbr[i]);
          return;
        }
        if (direction == FORWARDS) {
          n->queue()->enque(qi, QUEUE_TAIL);
          qi->insert(&animations_);
          check_monitors(qi);
        } else {
          qi = n->queue()->remove(e.pe.pkt);
          delete qi;
        }
      }
      delete mbr;    
      break;
    }

    case 'D': {
      n = lookupNode(e.pe.dst);
      if (n == NULL) {
        fprintf(stderr, "Bad node id %d for session deque event\n",
          e.pe.dst);
        return;
      }
      if (direction == FORWARDS) {
        q = n->queue()->remove(e.pe.pkt);
        delete q;
      } else {
        QueueItem *qi = new QueueItem(e.pe.pkt, e.time, e.offset);
        qi->paint(paint_[e.pe.pkt.attr & paintMask_]);
        n->queue()->enque(qi, QUEUE_HEAD);
        qi->insert(&animations_);
        check_monitors(qi);
      }
      break;
    }

    case 'P':  // session drop
      // Get packet to drop.
      if (direction == FORWARDS) {
        // fprintf(stderr, "drop on %d -> %d\n", e.pe.src, e.pe.dst);
        n = lookupNode(e.pe.dst);
        if (n == 0)
          return;
        q = 0;
        if (n->queue() != 0) {
          // Remove packet from session queue 
          q = n->queue()->remove(e.pe.pkt);
          if (q == 0) {
            // No such packet in queue??? 
            fprintf(stderr, "No packet drop %id in queue\n",
                            e.pe.pkt.id);
            return;
          }
          q->position(x,y);
          pno = q->paint();
          delete q;
        }
        
      // Compute the point at which the dropped packet disappears.
      // Let's just make this sufficiently far below the lowest
      // thing on the screen.
    
      // Watch out for topologies that have all their nodes lined
      // up horizontally. In this case, nymin_ == nymax_ == 0.
      // Set the bottom to -0.028. This was chosen empirically.
      // The nam display was set to the maximum size and the lowest
      // position on the display was around -0.028.
     
      float bot;
      if (nymin_ - nymax_ < 0.01)
        bot = nymin_ - 10 * n->size() ;
      else
        bot = nymin_ - (nymax_ - nymin_);
    
      Drop *d = new Drop(x, y, bot, n->size()*0.5, /* This is a hack */
                         e.time, e.offset, e.pe.pkt);
      d->paint(pno);
      d->insert(&animations_);
      check_monitors(d);
      break;
    } else {
      /*direction is BACKWARDS - need to create the packet*/
      //fprintf(stderr, "Packet drop backwards\n");
    }

    case 'G': {
      /* Group event */
      Group *grp = lookupGroup(e.ge.src);
      if (grp == NULL) {
        grp = new Group(e.ge.grp.name, e.ge.src);
        add_group(grp);
      }
      
      if (e.ge.grp.flag == GROUP_EVENT_JOIN) {
        grp->join(e.ge.grp.mbr);
        // XXX
        // Hard-coded queue angle for session queues. :(
        // Create session queue here because they are not like
        // traditional queues which are created when nam
        // started. Group member may dynamically join/leave,
        // so may session queues. We create them here because 
        // there's a 1-1 relationship between join and creating
        // session queues.
        n = lookupNode(e.ge.grp.mbr);
        if (n == NULL) {
          fprintf(stderr, "Bad node %d for group event\n",
            e.ge.grp.mbr);
          return;
        }
        // Need more consideration on the placement of these queues
        Queue *q = new Queue(0.5);
        q->next_ = queues_;
        queues_ = q;
        n->add_sess_queue(e.ge.src, q);
      } else if (e.ge.grp.flag == GROUP_EVENT_LEAVE)
        // No deletion of session queues.
        grp->leave(e.ge.grp.mbr);
      break;
    }
    case 'l': 
      /*link event*/
      // if src or dst = -1 this is a layout link (-t *)
      // so skip over it
      if (e.le.src == -1 || e.le.dst == -1)
        return;

      ehn = lookupEdgeHashNode(e.le.src, e.le.dst);
      if (ehn == 0) {
        // if edge doesn't exist try to create it dynamically
        ep = addEdge(e.le.src, e.le.dst, e);
        if (!ep) {
          fprintf(stderr, "Unable to create edge(%d,%d) dynamically.\n",
                           e.le.src, e.le.dst);
          return;
        }
        ehn = lookupEdgeHashNode(e.le.src, e.le.dst);
        do_relayout= true;
      }

      ehnrev = lookupEdgeHashNode(e.le.dst, e.le.src);
      if (ehnrev == 0) {
        // if edge doesn't exist try to create it dynamically
        ep = addEdge(e.le.dst, e.le.src, e);
        if (!ep) {
          fprintf(stderr, "Unable to create reverse edge(%d,%d) dynamically.\n",
                           e.le.dst, e.le.src);
          return;
        }
        ehnrev = lookupEdgeHashNode(e.le.dst, e.le.src);
        do_relayout = true;
      }

      if (do_relayout) {
        //relayout();
        relayoutNode(lookupNode(e.le.src));
        relayoutNode(lookupNode(e.le.dst));
        do_relayout = false;
      }
      /*note: many link events are bidirectional events so be careful to
        apply them to both a link and the reverse of it*/

      if (direction==FORWARDS)  {
        // Always save the color before the last DOWN event
        if (strncmp(e.le.link.state, "DOWN", 4)==0) {
          ehn->edge->set_down("red");
          ehnrev->edge->set_down("red");
        } else if (strncmp(e.le.link.state, "UP", 2)==0) {
          /* XXX Assume an UP event always follows a DOWN event */
          ehn->edge->set_up();
          ehnrev->edge->set_up();
        } else if (strncmp(e.le.link.state, "COLOR", 5) == 0) {
          ehn->edge->color((char *)e.le.link.color);
           ehnrev->edge->color((char *)e.le.link.color);
        } else if (strncmp(e.le.link.state, "DLABEL", 6) == 0) {
          ehn->edge->dlabel((char *)e.le.link.dlabel);
          ehnrev->edge->dlabel((char *)e.le.link.dlabel);
        } else if (strncmp(e.le.link.state, "DCOLOR", 6) == 0) {
          ehn->edge->dcolor((char *)e.le.link.dcolor);
          ehnrev->edge->dcolor((char *)e.le.link.dcolor);
        } else if (strncmp(e.le.link.state, "DIRECTION", 9) == 0) {
          ehn->edge->direction((char *)e.le.link.direction);
          ehnrev->edge->direction((char *)e.le.link.direction);
        } else if (strncmp(e.le.link.state, "DIRECTION", 9) == 0) {
          ehn->edge->direction((char *)e.le.link.direction);
          ehnrev->edge->direction((char *)e.le.link.direction);
        } 

      } else {
        if (strncmp(e.le.link.state, "UP", 2)==0) {
          ehn->edge->set_down("red");
          ehnrev->edge->set_down("red");
        } else if (strncmp(e.le.link.state, "DOWN", 4)==0) {
          ehn->edge->set_up();
        } else if (strncmp(e.le.link.state, "COLOR", 5) == 0) {
          ehn->edge->color((char *)e.le.link.oldColor);
          ehnrev->edge->color((char *)e.le.link.oldColor);
        } else if (strncmp(e.le.link.state, "DLABEL", 6) == 0) {
          ehn->edge->dlabel((char *)e.le.link.odlabel);
          ehnrev->edge->dlabel((char *)e.le.link.odlabel);
        } else if (strncmp(e.le.link.state, "DCOLOR", 6) == 0) {
          ehn->edge->dcolor((char *)e.le.link.odcolor);
          ehnrev->edge->dcolor((char *)e.le.link.odcolor);
        } else if (strncmp(e.le.link.state, "DIRECTION", 9) == 0) {
          ehn->edge->direction((char *)e.le.link.odirection);
          ehnrev->edge->direction((char *)e.le.link.odirection);
        } else if (strncmp(e.le.link.state, "DIRECTION", 9) == 0) {
          ehn->edge->direction((char *)e.le.link.odirection);
          ehnrev->edge->direction((char *)e.le.link.odirection);
        }
    }
      break;
    case 'n':
      /* node event */
      // Return if node has -t * 
      // Ths node is only used for initial layout
      if (e.ne.src == -1)
        return;

      n = lookupNode(e.ne.src);
      if (n == 0) {
        // if node doesn't exist try to create it dynamically
        n = addNode(e);
        if (!n)
          return;
      }
      if (direction==FORWARDS) {
        if (strncmp(e.ne.node.state, "DOWN", 4)==0) {
                n->set_down("gray");
        } else if (strncmp(e.ne.node.state, "UP", 2)==0) {
                n->set_up();
        } else if (strncmp(e.ne.node.state, "COLOR", 5) == 0) {
                // normal color can be defined by user
                n->color((char *)e.ne.node.color);
                n->lcolor((char *)e.ne.node.lcolor);
        } else if (strncmp(e.ne.node.state, "DLABEL", 6) == 0) {
                n->dlabel((char *)e.ne.node.dlabel); 
        } else if (strncmp(e.ne.node.state, "DCOLOR", 6) == 0) {
                n->dcolor((char *)e.ne.node.dcolor);
        } else if (strncmp(e.ne.node.state, "DIRECTION", 9) == 0) {
                n->direction((char *)e.ne.node.direction);
        }
      } else {
        if (strncmp(e.ne.node.state, "UP", 4)==0) {
                n->set_down("gray");
        } else if (strncmp(e.ne.node.state, "DOWN", 2)==0) {
                n->set_up();
        } else if (strncmp(e.ne.node.state, "COLOR", 5) == 0) {
                n->color((char *)e.ne.node.oldColor);
                n->lcolor((char *)e.ne.node.olcolor);
        }  else if (strncmp(e.ne.node.state, "DLABEL", 6) == 0) {
                n->dlabel((char *)e.ne.node.odlabel);
        } else if (strncmp(e.ne.node.state, "DCOLOR", 6) == 0) {
                n->dcolor((char *)e.ne.node.odcolor);
        } else if (strncmp(e.ne.node.state, "DIRECTION", 9) == 0) {
                n->direction((char *)e.ne.node.odirection);
        }
      }  
      break;

    case 'm':
      /* node mark event */
      NodeMark *cm;
      n = lookupNode(e.me.src);
      if (n == 0)
        return;
      cm = n->find_mark((char *) e.me.mark.name);
      if (direction == FORWARDS) {
        if (e.me.mark.expired == 0) {
          /* once created, a node mark cannot be changed*/
          if (cm == NULL) 
            n->add_mark((char *)e.me.mark.name, 
                        (char *)e.me.mark.color,
                        (char *)e.me.mark.shape);
        } else 
          /* expired */
          n->delete_mark((char *)e.me.mark.name);
      } else {
        /* 
         * backward: 
         * (1) create it if expired == 1
         * (2) delete it if expired == 0
         */
        if (e.me.mark.expired == 0) 
          n->delete_mark((char *)e.me.mark.name);
        else {
          /* re-create the circle */
          if (cm == NULL)
            n->add_mark((char *)e.me.mark.name, 
            (char *)e.me.mark.color,
            (char *)e.me.mark.shape);
        }
      }
      break;

    case 'R':
      // route event
      if (((e.re.route.expired==0)&&(direction==FORWARDS))||
          ((e.re.route.expired==1)&&(direction==BACKWARDS))) {
         // this is a new route
        n = lookupNode(e.re.src);
        if (n == 0)
          return;

        ehn = lookupEdgeHashNode(e.re.src, e.re.dst);
        if (ehn == 0)
          return;
        int oif=1;
        if (strncmp(e.re.route.mode, "iif", 3)==0)
          oif=0;
        r = new Route(n, ehn->edge, e.re.route.group, e.re.route.pktsrc, 
                      e.re.route.neg, oif, e.re.route.timeout, now);
        n->add_route(r);
        n->place_route(r);
        r->insert(&animations_);
        r->paint(paint_[e.re.route.group & paintMask_]);
        check_monitors(r);
      } else {
        // an old route expired
        n = lookupNode(e.re.src);
        if (n == 0)
          return;
        
        // src and dst are node ids 
        ehn = lookupEdgeHashNode(e.re.src, e.re.dst);
        if (ehn == 0)
          return;
        int oif = 1;
        if (strncmp(e.re.route.mode, "iif", 3) == 0) {
          oif=0;
        }
        r = n->find_route(ehn->edge, e.re.route.group,
                          e.re.route.pktsrc, oif);
        if (r == 0) {
          fprintf(stderr, "nam: attempt to delete non-existent route\n");
          abort();
        }
        n->delete_route(r);
        delete r;
      }
      break;

    case 'd':
      add_drop(e, now, direction);
  }
}

//---------------------------------------------------------------------
//  void
// NetModel::add_drop(const TraceEvent &e, double now, int direction)
//   - This method adds a packet drop animation to the animations_ list
//   - Packet drops can occur from queues and edges.  If the queue is 
//     not being displayed the packet is dropped from the node
//     position.
//   - If the animation direction is BACKWARDS a packet should be
//     created but it appears that this code does not do that.
//---------------------------------------------------------------------
void
NetModel::add_drop(const TraceEvent &e, double now, int direction) {
  EdgeHashNode *ehn;
  QueueItem *q;
  Packet *p;
  Lan *lan;
  float x, y;
  int pno;  // paint number (color with which to draw)
  
  // Get packet to drop. 
  if (direction == FORWARDS) {
    ehn = lookupEdgeHashNode(e.pe.src, e.pe.dst);
    if (ehn == 0) {
     return;
    }

    q = 0;
    if (ehn->queue != 0) {
      // if there's a queue, try removing it from the queue first
      // queue drops are more common than link drops...
      q = ehn->queue->remove(e.pe.pkt);
    }

    if (q == 0) {
      // perhaps it's a link packet drop
      p = lookupPacket(e.pe.src, e.pe.dst, e.pe.pkt.id);
      if (p != NULL) {
        // it was a link packet drop
        p->position(x, y, now);
        ehn->edge->DeletePacket(p);
        pno = p->paint();
        delete p;
      } else if ((lan = lookupLan(e.pe.src)) != NULL) {
        /* 
         * If it's a Lan (selective) packet drop, which means
         * the packet is only dropped on some of the lan links,
         * register the packet on the lan and drop it when the
         * packet is actually transmitted to that lan link.
         *
         * When the packet is actually dropped, this function will be 
         * called again, but at that time the packet will be actually
         * on the link and this code will not be executed.
         */
        lan->register_drop(e);
        return;
      } else {
        // probably it was a queue drop, but we're not displaying
        // that queue
       
        // It's possible that this packet is dropped directly from 
        // the queue, even before the enqT_ module. In this case, 
        // we should still produce a packet drop; we use the position
        // of the node to generate the drop.
        Node *s = lookupNode(e.pe.src);
        if (s == NULL) {
          fprintf(stderr, "NetModel::add_drop(): cannot find src node for packet drop.\n");
          abort();
        }
        x = s->x();
        y = s->y();
        pno = paint_[e.pe.pkt.attr & paintMask_];
      }
    } else {
      // packet dropped from queue
      // get x,y position of queue item
      q->position(x, y);
      pno = q->paint();
      delete q;
    }

    /*
     * Compute the point at which the dropped packet disappears.
     * Let's just make this sufficiently far below the lowest
     * thing on the screen.
     *
     * Watch out for topologies that have all their nodes lined
     * up horizontally. In this case, nymin_ == nymax_ == 0.
     * Set the bottom to -0.028. This was chosen empirically.
     * The nam display was set to the maximum size and the lowest
     * position on the display was around -0.028.
     */
    float bottom;
    if (nymin_ - nymax_ < 0.01) {
      bottom = nymin_ - 20.0 * ehn->edge->PacketHeight() ;
    } else {
      bottom = nymin_ - (nymax_ - nymin_);
    }
    
    // The drop animation is taken care of by the drop class
    Drop * d = new Drop(x, y, bottom, 4 * ehn->edge->PacketHeight(),
                        e.time, e.offset, e.pe.pkt);
    d->paint(pno);
    d->insert(&animations_);
    check_monitors(d);
    return;
  } else {
    // direction is BACKWARDS - need to create the packet
    Lan *lan = lookupLan(e.pe.src);
    if (lan != NULL) {
       // We need to remove drop status in lans
      lan->remove_drop(e);
      //fprintf(stderr, "lan dropped packet %d is removed on lan link %d->%d\n",
      //                e.pe.pkt.id, e.pe.src, e.pe.dst);
      return;
    }
  }
}


//----------------------------------------------------------------------------
// Node *
// NetModel::addNode(const TraceEvent &e)
//   - adds a node to the netmodel getting configuration info from the fields
//     in the TraceEvent
//----------------------------------------------------------------------------
Node *
NetModel::addNode(const TraceEvent &e)  {
  Node * n = NULL;
  char src[32];

  if (e.tt == 'n') {
    sprintf(src, "%d", e.ne.src);
    n = lookupNode(e.ne.src);
    // And remove them to be replaced by this node
    if (n != NULL) {
      fprintf(stderr, "Skipping duplicate node %s definition. \n", src);
      //removeNode(n);
    }

    // Determine Node Type
    if (!strncmp(e.ne.mark.shape, "circle",6)) {
      n = new CircleNode(src, e.ne.size);
    } else if (!strncmp(e.ne.mark.shape, "box", 3) || 
               !strncmp(e.ne.mark.shape, "square", 6)) {
      n = new BoxNode(src, e.ne.size);
    } else if (!strncmp(e.ne.mark.shape, "hexagon",7)) { 
      n = new HexagonNode(src, e.ne.size);
    } else {
      return NULL;
    }

    // Check for wireless node
    if (e.ne.wireless) {
      n->wireless_ = true;
      //fprintf(stderr, "We have wireless nodes :-) !!!\n");
    }

    // Node Address
    //  May need to check for no address passed in
    n->setaddr(e.ne.node.addr);
    addAddress(n->num(), e.ne.node.addr);

    // Node Color
    n->init_color(e.ne.node.color);
    n->lcolor(e.ne.node.lcolor);

    // dlabel initilization
    n->dlabel(e.ne.node.dlabel);

    // Set X,Y cordinates
    n->place(e.ne.x, e.ne.y);
    
    // Add Node to drawables list
    n->next_ = nodes_;
    nodes_ = n;
    n->Animation::insert(&drawables_);

    // Set Packet size to be running average of the last 5 nodes (25% of node size)
    packet_size_ = (4.0 * packet_size_ + e.ne.size*0.25)/5.0;
  }
  return (n);
}

//----------------------------------------------------------------------------
// Edge * 
// NetModel::addEdge(int argc, const char *const *argv)
//
//  <net> link <src> <dst> <bandwidth> <delay> <angle>
//  Create a link/edge between the specified source
//  and destination. Add it to this NetModel's list
//  of drawables and to the source's list of links.
//----------------------------------------------------------------------------
Edge * 
NetModel::addEdge(int argc, const char *const *argv) {
  Node * src, * dst;
  Edge * edge = NULL;
  double bandwidth, delay, length, angle;

  if (strcmp(argv[1], "link") == 0) {
    src = lookupNode(atoi(argv[2]));
    dst = lookupNode(atoi(argv[3]));
    
    if (src && dst) {
      bandwidth = atof(argv[4]);
      delay = atof(argv[5]);
      length = atof(argv[6]);
      angle = atof(argv[7]);
                        
      //enlarge link if the topology is a mixture of
      //wired and wireless network
      if (wireless_) {
        length = delay * WIRELESS_SCALE ;
      }

      edge = new Edge(src, dst, packet_size_, bandwidth, delay, length, angle, wireless_);
      edge->init_color("black");
      enterEdge(edge);
      edge->Animation::insert(&drawables_);
      src->add_link(edge);
    }
  }
  return edge;
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
Edge * 
NetModel::addEdge(int src_id, int dst_id, const TraceEvent &e) {
  Node *src, *dst;
  Edge * edge = NULL;
  double bandwidth, delay, length, angle;

  if (e.tt == 'l') {
    src = lookupNode(src_id);
    dst = lookupNode(dst_id);
    
    if (src && dst) {
      bandwidth = e.le.link.rate;
      delay = e.le.link.delay;
      length = e.le.link.length;
      angle = e.le.link.angle;
                        
      //enlarge link if the topology is a mixture of
      //wired and wireless network
      if (wireless_) {
        length = delay * WIRELESS_SCALE ;
      }

      edge = new Edge(src, dst, packet_size_, bandwidth, delay, length, angle, wireless_);
      edge->init_color("black");
      enterEdge(edge);
      edge->Animation::insert(&drawables_);
      src->add_link(edge);
    }
  }
  return edge;
}

void NetModel::addView(NetView* p)
{
	p->next_ = views_;
	views_ = p;
}

//----------------------------------------------------------------------
// Node * 
// NetModel::lookupNode(int nn) const
//----------------------------------------------------------------------
Node *
NetModel::lookupNode(int nn) const {
  for (Node* n = nodes_; n != 0; n = n->next_)
    if (n->num() == nn)
      return (n);
  return NULL;
}


//----------------------------------------------------------------------
// Edge *
// NetModel::lookupEdge(int source, int destination) const
//----------------------------------------------------------------------
Edge *
NetModel::lookupEdge(int source, int destination) const {
	EdgeHashNode * edge_hash_node;
	edge_hash_node = lookupEdgeHashNode(source, destination);
	return edge_hash_node->edge;
}


void NetModel::removeNode(Node *n) 
{
	Node *p, *q;
	// Remove node n from nodes_ list, then delete it
	for (p = nodes_; p != 0; q = p, p = p->next_)
		if (p == n)
			break;
	if (p == nodes_)
		nodes_ = p->next_;
	else
		q->next_ = p->next_;
	delete p;
}

Agent *NetModel::lookupAgent(int id) const
{
	for (Node* n = nodes_; n != 0; n = n->next_) 
		for(Agent* a= n->agents(); a != 0; a = a->next_) 
			if (a->number() == id) 
				return (a);
	return (0); 
}

Lan *NetModel::lookupLan(int nn) const
{
	for (Lan* l = lans_; l != 0; l = l->next_)
		if (l->num() == nn)
			return (l);
	/* XXX */
	//fprintf(stderr, "nam: no such lan %d\n", nn);
	//exit(1);
	return (0);// make visual c++ happy
}

Packet *NetModel::newPacket(PacketAttr &pkt, Edge *e, double time)
{
  /*this is called when we get a triggered event such as a packet
    getting duplicated within a LAN*/
  Packet *p = new Packet(e, pkt, time, e->txtime(pkt.size), 0);
  p->insert(&animations_);
  p->paint(paint_[pkt.attr & paintMask_]);
  check_monitors(p);
  return p;
}

Packet *NetModel::lookupPacket(int src, int dst, int id) const
{
  EdgeHashNode *h = lookupEdgeHashNode(src, dst);
  if (h == 0)
    return NULL;
  int ctr=0;
  for (Packet *p=h->edge->packets(); p!=NULL; p=p->next())
    {
#define PARANOID
#ifdef PARANOID
      ctr++;
      if (ctr>h->edge->no_of_packets()) abort();
#endif
      if (p->id() == id)
	return p;
    }
  /*have to fail silent or we can't cope with link drops when doing settime*/
  return 0;
}

/* Do not delete groups, because they are not explicitly deleted */
int NetModel::add_group(Group *grp)
{
	int newEntry = 1;
	Tcl_HashEntry *he = Tcl_CreateHashEntry(grpHash_,
	                                        (const char *)grp->addr(), 
	                                        &newEntry);
	if (he == NULL)
		return -1;
	if (newEntry) {
		Tcl_SetHashValue(he, (ClientData)grp);
		nGroup_++;
	}
	return 0;
}

Group* NetModel::lookupGroup(unsigned int addr)
{
	Tcl_HashEntry *he = Tcl_FindHashEntry(grpHash_, (const char *)addr);
	if (he == NULL)
		return NULL;
	return (Group *)Tcl_GetHashValue(he);
}

// Remove a view from the views link, but not delete it
void NetModel::remove_view(View *v)
{
	View *p, *q;
	p = q = views_;
	if (p == v) {
		views_ = p->next_;
		return;
	}
	while (p != NULL) {
		q = p;
		p = p->next_;
		if (p == v) {
			q->next_ = p->next_;
			return;
		}
	}
}

//----------------------------------------------------------------------
// int
// NetModel::command(int argc, const char *const *argv)
//   - Parses tcl commands (hook to enter c++ code from tcl)
//----------------------------------------------------------------------
int NetModel::command(int argc, const char *const *argv) {
	Tcl& tcl = Tcl::instance();
	int i;
	Node * node;
	double time;
	
	if (argc == 2) {
		if (strcmp(argv[1], "layout") == 0) {
			/*
			 * <net> layout
			 * Compute reasonable defaults for missing node or edge
			 * sizes based on the maximum link delay. Lay out the
			 * nodes and edges as specified in the layout file.
			 */
			scale_estimate();
			placeEverything();
			return (TCL_OK);
		}
		if (strcmp(argv[1], "showtrees") == 0) {
			/*
			 * <net> showtrees
			 */
			color_subtrees();
			for (View* p = views_; p != 0; p = p->next_) {
				p->draw();
			}
			return (TCL_OK);
		}
		if (strcmp(argv[1],"resetFilter")==0) {
		        resetf_ = 1 ;
	                selectedSrc_ = -1 ;
		        selectedDst_ = -1 ;
		        selectedFid_ = -1 ;
	                colorSrc_ = -1 ;
		        colorDst_ = -1 ;
		        colorFid_ = -1 ;
	                hideSrc_ = -1 ;
		        hideDst_ = -1 ;
		        hideFid_ = -1 ;
	                for (i = 0; i < PTYPELEN; ++i) {
	                   selectedTraffic_[i] = '\0' ;
	                   colorTraffic_[i] = '\0' ;
	                   hideTraffic_[i] = '\0' ;
                        }

			return (TCL_OK);
		}

	} else if (argc == 3) {
		if (strcmp(argv[1], "incr-nodesize") == 0) {
			/*
			 * <net> incr-nodesize <factor>
			 */
			node_sizefac_ *= atof(argv[2]);
			for (Node *n = nodes_; n != 0; n = n->next_)
				for (Edge *e=n->links(); e != 0; e = e->next_)
					e->unmark();
			scale_estimate();
			placeEverything();
			for (View *p = views_; p != 0; p = p->next_)
				if ((p->width() > 0) && (p->height() > 0)) {
					p->redrawModel();
					p->draw();
				}
			return (TCL_OK);
		}
		if (strcmp(argv[1], "decr-nodesize") == 0) {
			node_sizefac_ /= atof(argv[2]);
			for (Node *n = nodes_; n != 0; n = n->next_)
				for (Edge *e=n->links(); e != 0; e = e->next_)
					e->unmark();
			scale_estimate();
			placeEverything();
			for (View *p = views_; p != 0; p = p->next_)
				if ((p->width() > 0) && (p->height() > 0)) {
					p->redrawModel();
					p->draw();
				}
			return(TCL_OK);
		}

		if (strcmp(argv[1], "updateNodePositions") == 0) {
			time = strtod(argv[2], NULL);
			for (node = nodes_; node; node = node->next_) {
				node->updatePositionAt(time);
				moveNode(node);  // This updates the links and agents connected to the node
			}
			return TCL_OK;
		}

		if (strcmp(argv[1], "view") == 0) {
			/*
			 * <net> view <viewName>
			 * Create the window for the network layout/topology.
			 * Used for nam editor
			 */

			EditView *v = new EditView(argv[2], this, 300, 700);
			v->next_ = views_;
			views_ = v;
			return (TCL_OK);
		}

		if (strcmp(argv[1], "psview") == 0) {
			/*
			 * <net> PSView <fileName>
			 * Print the topology to a file
			 */
			PSView *v = new PSView(argv[2], this);
			v->render();
			delete(v);
			return (TCL_OK);
		}
		if (strcmp(argv[1], "testview") == 0) {
			/*
			 * Added for nam validation test 
			 */
			TestView *v = new TestView(argv[2], this);
			v->render();
			delete(v);
			return (TCL_OK);
		}
		if (strcmp(argv[1], "editview") == 0) {
			/*
			 * <net> editview <viewname>
			 */
			EditView *v = new EditView(argv[2], this);
			v->next_ = views_;
			views_ = v;
			return (TCL_OK);
		}
		if (strcmp(argv[1],"savelayout")==0) {
		        save_layout(argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"select-traffic")==0) {
		        strcpy(selectedTraffic_,argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"select-src")==0) {
		        selectedSrc_ = atoi(argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"select-dst")==0) {
		        selectedDst_ = atoi(argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"select-fid")==0) {
		        selectedFid_ = atoi(argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"hide-traffic")==0) {
		        strcpy(hideTraffic_,argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"hide-src")==0) {
		        hideSrc_ = atoi(argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"hide-dst")==0) {
		        hideDst_ = atoi(argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"hide-fid")==0) {
		        hideFid_ = atoi(argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"color-traffic")==0) {
		        resetf_ = 0 ;
		        strcpy(colorTraffic_,argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"color-src")==0) {
		        resetf_ = 0 ;
		        colorSrc_ = atoi(argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"color-dst")==0) {
		        resetf_ = 0 ;
		        colorDst_ = atoi(argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"color-fid")==0) {
		        resetf_ = 0 ;
		        colorFid_ = atoi(argv[2]);
			return (TCL_OK);
		}
		if (strcmp(argv[1],"select-color")==0) {
			Paint *paint = Paint::instance();
		        selectedColor_ = paint->lookup(argv[2], 1);
			if (selectedColor_ < 0) {
			  fprintf(stderr,"%s color: no such color: %s\n",
				  argv[0], argv[2]);
			  selectedColor_ = paint->lookup("black",1);
			  if (selectedColor_ < 0) {
			    tcl.resultf("%s no black! - bailing");
			    return (TCL_ERROR);
			  }
			}
			return (TCL_OK);
    }
		
	  if (strcmp(argv[1], "node") == 0) {

//	 else if (argc == 3 && strcmp(argv[1], "node") == 0)
		/*
		 * <net> node <name> <shape> <color> <addr> [<size>]
		 * Create a node using the specified name
		 * and the default size and insert it into this
		 * NetModel's list of drawables.
		 */
		//Node* n = addNode(argc, argv);
      
      char * line = new char[strlen(argv[2])];
      strncpy(line, argv[2],strlen(argv[2]));
      parsetable_.parseLine(line);
  		Node * node = addNode(traceevent_);
      delete line;
  		if (node) {
  			return (TCL_OK);
  		} else {
  			tcl.resultf("Unable to create Node %s.", argv[2]);
  			return (TCL_ERROR);
  		}
    }


	} else if (argc >= 4 && strcmp(argv[1], "agent") == 0) {
		// Create a new agent
    
		/*
		 * <net> agent <label> <role> <node> <flowcolor> <winInit> 
		 *             <win> <maxcwnd>
		 *             <tracevar> <start> <producemore> <stop>
		 * <net> agent <label> <role> <node> <flowcolor> <packetSize>
		 *       <interval> <start> <stop>
		 */
		int node = atoi(argv[4]);
		int partner_num;
		Node *n = lookupNode(node);
    
		if (n == 0) {
			tcl.resultf("Node %d doesn't exist.", node);
			return (TCL_ERROR);
		}

		Agent *a = new BoxAgent((char *)argv[2], n->size());
		if (a == 0) {
			tcl.resultf("Cannot create Agent %s exist on Node %d",
				    argv[2], node);
			return (TCL_ERROR);
		}

		placeAgent(a, n);
		a->node_ = n;
		n->add_agent(a);
    
		a->AgentRole_ = (atoi(argv[3]));

		if (a->AgentRole_ == 20) {
			a->setNumber(atoi(argv[5]));
			partner_num = (atoi(argv[6]));

			Agent *partner = lookupAgent(partner_num);
			if (partner) {
				a->AgentPartner_ = partner;
				partner->AgentPartner_ = a;
			}

		} else if ((a->AgentRole_ == 10) && 
			   (strcmp(argv[2], "CBR") == 0)) {
			if (strcmp(argv[5],"(null)")!=0)
				a->flowcolor((char *)argv[5]);
			a->packetSize(atoi(argv[6]));
			a->interval(atoi(argv[7]));
			a->startAt(atof(argv[8]));
			a->stopAt(atof(argv[9]));
			a->setNumber(atoi(argv[10]));
			partner_num = (atoi(argv[11]));

			Agent *partner = lookupAgent(partner_num);
			if (partner) {
				a->AgentPartner_ = partner;
				partner->AgentPartner_ = a;
			}

		} else if (a->AgentRole_ != 0) {
			if (strcmp(argv[5],"(null)")!=0)
				a->flowcolor((char *)argv[5]);
			a->windowInit(atoi(argv[6]));
			a->window(atoi(argv[7]));
			a->maxcwnd(atoi(argv[8]));
			a->tracevar((char *)argv[9]);
			a->startAt(atof(argv[10]));
			a->produce(atoi(argv[11]));
			a->stopAt(atof(argv[12]));
			a->setNumber(atoi(argv[13]));
			partner_num = (atoi(argv[14]));

			Agent *partner = lookupAgent(partner_num);
			if (partner) {
				a->AgentPartner_ = partner;
				partner->AgentPartner_ = a;
			}
		}
		a->Animation::insert(&animations_);
		return (TCL_OK);

	} else if (argc == 4) {
		if (strcmp(argv[1], "new_monitor_agent") == 0) {
			/* 
			 * <net> new_monitor_agent <node_id> <agent_name>
			 */
			int node = atoi(argv[2]);
			Node *n = lookupNode(node);
			if (n == 0) {
				tcl.resultf("Node %d doesn't exist.", node);
				return (TCL_ERROR);
			}
			Agent *a = n->find_agent((char *)argv[3]);
			if (a == 0) {
				tcl.resultf("Agent %s not exist at node %d",
					    argv[3], node);
				return (TCL_ERROR);
			}
			tcl.resultf("%d", add_monitor(a));
			return (TCL_OK);
		}

		if (strcmp(argv[1], "color") == 0) {
			/*
			 * <net> color <packetClass> <colorName>
			 * Display packets of the specified class using
			 * the specified color.
			 */
			int c = atoi(argv[2]);
//  			if ((unsigned int)c > 1024) {
//  				tcl.resultf("%s color: class %d out of range",
//  					    argv[0], c);
//  				return (TCL_ERROR);
//  			}
			// Convert this color index to [0,255], so that 
			// it matches correctly with packet flow ids.
			Paint *paint = Paint::instance();
			if (c > nclass_) {
				int n, i;
				for (n = nclass_; n < c; n <<= 1);
				int *p = new int[n];
				for (i = 0; i < nclass_; ++i)
					p[i] = paint_[i];
				delete paint_;
				paint_ = p;
				nclass_ = n;
				paintMask_ = nclass_ - 1;
				int pno = paint->thin();
				for (; i < n; ++i)
					paint_[i] = pno;
			}
			int pno = paint->lookup(argv[3], 1);
			if (pno < 0) {
			  fprintf(stderr,"%s color: no such color: %s\n",
				  argv[0], argv[3]);
			  pno = paint->lookup("black",1);
			  if (pno < 0) {
			    tcl.resultf("%s no black! - bailing");
			    return (TCL_ERROR);
			  }
			}
			paint_[c] = pno;
			oldpaint_[c] = pno;

			return (TCL_OK);
		}
    
		if (strcmp(argv[1], "ncolor") == 0) {
			/*
			 * <net> ncolor <node> <colorName>
			 * set color of node to the specified color.
			 */
			Paint *paint = Paint::instance();
			Node *n = lookupNode(atoi(argv[2]));
			if (n == NULL) {
				fprintf(stderr, "No such node %s\n", argv[2]);
				exit(1);
			}
			int pno = paint->lookup(argv[3], 3);
			if (pno < 0) {
			  fprintf(stderr,"%s ncolor: no such color: %s\n",
				  argv[0], argv[3]);
			  int pno = paint->lookup("black",1);
			  if (pno < 0) {
			    tcl.resultf("%s no black! - bailing");
			    return (TCL_ERROR);
			  }
			}
			n->paint(pno);
			return (TCL_OK);
		}



      
  // ----- 5 tcl arguments ------
	} else if (argc == 5) {
		if (strcmp(argv[1], "select-pkt") == 0) {
			selectPkt(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
			return(TCL_OK);
		}


		if (strcmp(argv[1], "lookupColorName") == 0) {
			/*
			 * <net> lookupColorName red green blue
			 * get color name from its rgb value 
			 */   
			 Paint *paint = Paint::instance();
			 int r = atoi(argv[2]);
			 int g = atoi(argv[3]);
			 int b = atoi(argv[4]);
			 tcl.resultf("%s",paint->lookupName(r,g,b));
			 return(TCL_OK);
		}


		if (strcmp(argv[1], "queue") == 0) {
			/*
			 * <net> queue <src> <dst> <angle>
			 * Create a queue for the edge from 'src' to 'dst'.
			 * Add it to this NetModel's queue list.
			 * Display the queue at the specified angle from
			 * the edge to which it belongs.
			 */
			int src = atoi(argv[2]);
			int dst = atoi(argv[3]);
			EdgeHashNode *h = lookupEdgeHashNode(src, dst);
			if (h == 0) {
				tcl.resultf("%s queue: no such edge (%d,%d)",
					    argv[0], src, dst);
				return (TCL_ERROR);
			}
			/* XXX can we assume no duplicate queues? */
			double angle = atof(argv[4]);
			Edge *e = h->edge;
			angle += e->angle();
			Queue *q = new Queue(angle);
			h->queue = q;
			q->next_ = queues_;
			queues_ = q;
			return (TCL_OK);
		}


		if (strcmp(argv[1], "ecolor") == 0) {
			/*
			 * <net> ecolor <src> <dst> <colorName>
			 * set color of edge to the specified color.
			 */
			Paint *paint = Paint::instance();
			EdgeHashNode* h = lookupEdgeHashNode(atoi(argv[2]), atoi(argv[3]));
			if (h == 0) {
				tcl.resultf("%s ecolor: no such edge (%s,%s)",
					    argv[0], argv[2], argv[3]);
				return (TCL_ERROR);
			}
			int pno = paint->lookup(argv[4], 3);
			if (pno < 0) {
			  fprintf(stderr,"%s ncolor: no such color: %s\n",
				  argv[0], argv[3]);
			  pno = paint->lookup("black",1);
			  if (pno < 0) {
			    tcl.resultf("%s no black! - bailing");
			    return (TCL_ERROR);
			  }
			}
			h->edge->paint(pno);
			return (TCL_OK);
		}


		if (strcmp(argv[1], "edlabel") == 0) {
			/*
			 * <net> edlabel <src> <dst> <colorName>
			 * set label of edge.
			 */
			EdgeHashNode* h = lookupEdgeHashNode(atoi(argv[2]), 
						     atoi(argv[3]));
			if (h == 0) {
				tcl.resultf("%s ecolor: no such edge (%s,%s)",
					    argv[0], argv[2], argv[3]);
				return (TCL_ERROR);
			}
			if (strcmp(argv[4],"(null)")!=0) {
				h->edge->dlabel((char *)argv[4]);
				// XXX Should never set edge size outside
				// scale_estimate()!!!!
//  				h->edge->size(25.0);
			}
			return (TCL_OK);
		}


		if (strcmp(argv[1], "lanlink") == 0) {
			/*
			 * <net> lanlink <src> <lan> <angle>
			 * Create a link/edge between the specified source
			 * and a lan.
			 */
			Node * src = lookupNode(atoi(argv[2]));
			Lan * lan = lookupLan(atoi(argv[3]));

			if (lan == NULL) {
				fprintf(stderr, "Error: lan %s does not exist.\n", argv[3]);
				exit(1);
			}

			double angle = atof(argv[4]);
			double bw = lan->bw();
			double delay = (lan->delay())/2.0;

			Edge *e1 = new Edge(src, lan->virtual_node(), packet_size_, bw, delay, 0, angle+1);
			e1->init_color("black");
			enterEdge(e1);

			e1->Animation::insert(&drawables_);
			src->add_link(e1);

			Edge *e2 = new Edge(lan->virtual_node(), src, packet_size_, bw, delay, 0, angle);
			e2->init_color("black");
			enterEdge(e2);
			e2->Animation::insert(&drawables_);
			lan->add_link(e2);
			return (TCL_OK);
		}


		if (strcmp(argv[1], "set_node_tclscript") == 0) {
			//------------------------------------------------------------------
			// $netModel set_node_tclscript $node_id $button_label $tcl_command
			//   - from tcl/node.tcl
			//          Animator instproc node_tclscript
			//------------------------------------------------------------------
			// Sets the tcl_script for a node to be run when the
			// start_info exec button is pressed.
				Node * node = lookupNode(atoi(argv[2]));
				if (node) {
					// The following strings are deleted by the node when it 
					// changes to another script or it is deleted
					node->setTclScript(argv[3], argv[4]);
					return TCL_OK;
				} else {
					return TCL_ERROR;
				}
			}



	} else if (argc == 6) {
		if (strcmp(argv[1], "lan") == 0) {
			/*
			 * <net> lan <name> <bandwidth> <delay> <angle>
			 * Create a link/edge between the specified source
			 * and destination. Add it to this NetModel's list
			 * of drawables and to the source's list of links.
			 */
			double bw = atof(argv[3]);
			double delay = atof(argv[4]);
			double angle = atof(argv[5]);

			Lan *l = new Lan(argv[2], this, packet_size_, bw, delay, angle);
			l->next_ = lans_;
			lans_ = l;
			l->insert(&drawables_);

			Node *n = l->virtual_node();
			n->next_ = nodes_;

			nodes_ = n;
			return (TCL_OK);
		}

	} else if (argc == 8) {
		if (strcmp(argv[1], "link") == 0) {
			/*
			 * <net> link <src> <dst> <bandwidth> <delay> <angle>
			 * Create a link/edge between the specified source
			 * and destination. Add it to this NetModel's list
			 * of drawables and to the source's list of links.
			 */
			Edge * edge = addEdge(argc,argv);
			if (!edge) {
				tcl.resultf("node %s or %s is not defined... ", argv[2], argv[3]);
				return TCL_ERROR;
			} else { 
				tcl.resultf("%g", edge->delay());
				return (TCL_OK);
			}
		}
	}
	/* If no NetModel commands matched, try the Object commands. */
	fprintf(stderr, "No matching command found for %s\n",argv[0]);
	return (TclObject::command(argc, argv));
}


void NetModel::selectPkt(int sData, int sRoute, int sMac) 
{
    showData_ = sData;
    showRouting_ = sRoute;
    showMac_ = sMac;
}



// ---------------------------------------------------------------------
// void
// NetModel::placeEdgeByAngle(Edge* e, Node* src) const {
// Place edges by their angles
// ---------------------------------------------------------------------
void 
NetModel::placeEdgeByAngle(Edge * e, Node * src) const {
  double nsin, ncos, x0, x1, y0, y1;
  Node * destination;
  EdgeHashNode * h;

  if (e->marked() == 0) {
    // get destination node
    destination = e->neighbor();

    SINCOSPI(e->angle(), &nsin, &ncos);

    x0 = src->x(e) + src->size() * ncos * 0.75;
    y0 = src->y(e) + src->size() * nsin * 0.75;
    x1 = destination->x(e) - destination->size() * ncos * 0.75;
    y1 = destination->y(e) - destination->size() * nsin * 0.75;

    e->place(x0, y0, x1, y1);
    
    // Place the queue here too.
    h = lookupEdgeHashNode(e->src(), e->dst());
    if (h->queue != 0) {
      h->queue->place(e->size(), e->x0(), e->y0());
    }

    e->mark();
  }
}

void NetModel::placeEdge(Edge * e, Node * src) const {
	if (e->marked() == 0) {
		double hyp, dx, dy;
		Node * dst = e->neighbor();
		dx = dst->x(e) - src->x(e);
		dy = dst->y(e) - src->y(e);
		hyp = sqrt(dx*dx + dy*dy);
		e->setAngle(atan2(dy,dx));
		double x0 = src->x(e) + src->size() * (dx/hyp) * .75;
		double y0 = src->y(e) + src->size() * (dy/hyp) * .75;
		double x1 = dst->x(e) - dst->size() * (dx/hyp) * .75;
		double y1 = dst->y(e) - dst->size() * (dy/hyp) * .75;
		e->place(x0, y0, x1, y1);
		
		/* Place the queue here too.  */
		EdgeHashNode *h = lookupEdgeHashNode(e->src(), e->dst());
		if (h->queue != 0)
			h->queue->place(e->size(), e->x0(), e->y0());

		e->mark();
	}
}

//----------------------------------------------------------------------
// void
// NetModel::placeAgent(Agent *a, Node *src) const
//----------------------------------------------------------------------
void 
NetModel::placeAgent(Agent *a, Node *src) const {
  double x0, y0, nsin, ncos;
  Agent * agents;
  Edge * links;
  int choices[8], i, ix;
  double angle;
  TrafficSource * traffic_source;

  if (a->marked() == 0) {
    if (a->angle()==NO_ANGLE) {
      if (a->edge()==NULL) {
        /* determine where to put the label so it
           won't overlap a link (if possible) */
        for(i = 0; i < 8; i++) {
          choices[i] = 1;
        }

        // Get list of agents attached to the src node
        agents = src->agents();

        // Loop through list of agents checking their locations
        while (agents != NULL) {
          angle = agents->angle();
          if (angle < 0) {
            angle = angle+2*M_PI;
          }
          ix = int(angle*4);
          if (ix < 8) {
            choices[ix] = 0;
          }
          agents=agents->next_;
        }

        // Get a list of links attached to the src node
        links = src->links();

        // Loop through the list checking their locations
        while (links!=NULL) {
          angle = links->angle();
          if (angle < 0) {
            angle = angle + 2*M_PI;
          }
          ix = int(angle * 4);
          if (ix < 8) {
            choices[ix] = 0;
          }
          links=links->next_;
        }

        // Choose an angle from the marked choices list
        if (choices[0] == 1) {
          a->angle(0);
        } else if (choices[4] == 1) {
          a->angle(1.0);
        } else if ((choices[1] == 1) && (choices[2] == 1)) {
          a->angle(0.5);
        } else if ((choices[6]==1)&&(choices[7]==1)) {
          a->angle(1.5);
        } else if (choices[1]==1) {
          a->angle(0.25);
        } else {
          a->angle(1.75);
        }
      } else {
        a->angle(a->edge()->angle() + 0.25);
      }
    }

    SINCOSPI(a->angle(), &nsin, &ncos);
    x0 = src->x() + src->size() * ncos * .75;
    y0 = src->y() + src->size() * nsin * .75;
    a->place(x0, y0);
    a->mark(1);

    // Place any traffic sources that are connected to this agent
    for (traffic_source = a->traffic_sources_;
         traffic_source;
         traffic_source = traffic_source->next_) {
      traffic_source->place();
    }
    
  }
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
NetModel::hideAgentLinks() {
	for (Node* n = nodes_; n != 0; n = n->next_) 
		for(Agent* a= n->agents(); a != 0; a = a->next_) 
			a->hideLink();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void NetModel::set_wireless() {
	wireless_ = 1 ;
	for (Node *n = nodes_; n != 0; n = n->next_)
		for (Edge *e=n->links(); e != 0; e = e->next_)
			e->unmark();
	scale_estimate();
	placeEverything();
	for (View *p = views_; p != 0; p = p->next_)
		if ((p->width() > 0) && (p->height() > 0)) {
			p->redrawModel();
			p->draw();
		}
}

//----------------------------------------------------------------------
// void
// NetModel::scale_estimate() {
//   - Compute reasonable defaults for missing node or edge sizes
//     based on the maximum link delay.
//----------------------------------------------------------------------
void
NetModel::scale_estimate() {
  /* Determine the maximum link delay. */
  // XXX Must use length(). This is essential for appropriate 
  // packet height when using a pre-made layout.
  double emax = 0.;
  Node *n;
  for (n = nodes_; n != 0; n = n->next_) {
    for (Edge* e = n->links(); e != 0; e = e->next_)
      if (e->length() > emax)
        emax = e->length();
  }

  /*store this because we need it for monitors*/
  node_size_ = node_sizefac_ * emax;

  /*
   * Check for missing node or edge sizes. If any are found,
   * compute a reasonable default based on the maximum edge
   * dimension.
   */
  for (n = nodes_; n != 0; n = n->next_) {
    if (n->size() <= 0.)
      n->size(node_size_);

    for (Edge* e = n->links(); e != 0; e = e->next_)
      if (e->size() <= 0.)
        e->size(.03 * emax);
  }
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void NetModel::placeEverything() {
  // If there is no fixed node, anchor the first one entered at (0,0).
  Node *n;
  Edge * e;
  int nodes_to_be_placed;

  for (n = nodes_; n != 0; n = n->next_) {
    n->mark(0);
    n->clear_routes();
  }

  if (nodes_) {
    //nodes_->place(0., 0.);
    nodes_->place(nodes_->x(), nodes_->y());
  }

  do {
    nodes_to_be_placed = 0;
    for (n = nodes_; n != 0; n = n->next_) {
      nodes_to_be_placed |= traverseNodeConnections(n);
    }
  } while (nodes_to_be_placed);

  // Place edges
  for (n = nodes_; n != 0; n = n->next_) { 
    for (e = n->links(); e != 0; e = e->next_) {
      placeEdgeByAngle(e, n);
    }
  }

  // After edges are laied out, place all routes.
  for (n = nodes_; n != 0; n = n->next_) {
    n->place_all_routes();
  }
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void NetModel::move(double& x, double& y, double angle, double d) const {
	double ncos, nsin;
	SINCOSPI(angle, &nsin, &ncos);
	x += d * ncos;
	y += d * nsin;
}

//----------------------------------------------------------------------
// int NetModel::traverseNodeConnections(Node* n)
//   - Traverse node n's neighbors and place them based on the
//     delay of their links to n.  The two branches of the if..else
//     are to handle unidirectional links -- we place ourselves if
//     we haven't been placed & our downstream neighbor has.
//----------------------------------------------------------------------
int NetModel::traverseNodeConnections(Node* n) {
  double x, y, distance;
  int placed_node = 0;

  for (Edge* e = n->links(); e != 0; e = e->next_) {
    Node *neighbor = e->neighbor();
    distance = e->length() + (n->size() + neighbor->size()) * 0.75;

    if (n->marked() && !neighbor->marked()) {
      x = n->x(e);
      y = n->y(e);
      move(x, y, e->angle(), distance);
      neighbor->place(x, y);

      placed_node |= traverseNodeConnections(neighbor);

      if (nymax_ < y)
        nymax_ = y;
      if (nymin_ > y)
        nymin_ = y;

    } else if (!n->marked() && neighbor->marked()) {
      x = neighbor->x(e);
      y = neighbor->y(e);
      move(x, y, e->angle(), -distance);
      n->place(x, y);
      placed_node = 1;
    }
  }
  return placed_node;
}

int NetModel::save_layout(const char *filename)
{
  FILE *file;
  Node *n;
  Edge *e;
  int ret;
  file=fopen(filename, "w");
  if (file==0)
  {
    fprintf(stderr, "nam: Couldn't open file: %s\n", filename);
    return -1;
  }
  for (n = nodes_; n != 0; n = n->next_)
  {
    ret = n->save(file);
    if (ret!=0) {
      fclose(file);
      return -1;
    }
  }
  for (n = nodes_; n != 0; n = n->next_)
    for(e= n->links(); e !=0; e = e->next_)
    {
      ret = e->save(file);
      if (ret!=0) {
	fclose(file);
	return -1;
      }
    }
  return(fclose(file));
}

void NetModel::color_subtrees()
{
  Node *n, *dst, *prevdst, *newdst;
  Edge* e;
  for (n = nodes_; n != 0; n = n->next_) {
    int ctr=0;
    for (e = n->links(); e != 0; e = e->next_)
      ctr++;
    if (ctr==1)
    {
      n->color("grey");
      n->links()->color("grey");
      dst=n->links()->neighbor();
      for (e = dst->links(); e != 0; e = e->next_)
	if (e->neighbor()==n)
	{
	  e->color("grey");
	  break;
	}
      ctr=2;
      prevdst=n;
      while(ctr==2) {
	ctr=0;
	for (e = dst->links(); e != 0; e = e->next_)
	  ctr++;
	if (ctr==2) {
	  dst->color("grey");
	  dst->links()->color("grey");
	  for (e = dst->links(); e != 0; e = e->next_)
	    if (e->neighbor()!=prevdst)
	    {
	      newdst=e->neighbor();
	      break;
	    }
	  e->color("grey");
	  for (e = newdst->links(); e != 0; e = e->next_)
	    if (e->neighbor()==dst)
	    {
	      e->color("grey");
	      break;
	    }
	  prevdst=dst;
	  dst=newdst;
	}
      }
    }
  }
}


//----------------------------------------------------------------------
// int
// NetModel::add_tag(Tag *tag)
//----------------------------------------------------------------------
int
NetModel::add_tag(Tag *tag) {
  int newEntry = 1;
  Tcl_HashEntry * he;
  he = Tcl_CreateHashEntry(tagHash_, (const char *)tag->name(), &newEntry);

  if (he == NULL) {
    return (TCL_ERROR);
  }

  if (newEntry) {
    Tcl_SetHashValue(he, (ClientData)tag);
    nTag_++;
  }

  tag->insert(&drawables_);

  return (TCL_OK);
}

// Remove a tag from netmodel, and clear all its memberships but not 
// actually delete it
void NetModel::delete_tag(const char *tn)
{
	Tcl_HashEntry *he = 
		Tcl_FindHashEntry(tagHash_, (const char *)tn);
	if (he != NULL) {
		// Do *NOT* delete tag here.
		Tcl_DeleteHashEntry(he);
		nTag_--;
	}
				  
}

Tag* NetModel::lookupTag(const char *tn)
{
	Tcl_HashEntry *he = Tcl_FindHashEntry(tagHash_, tn);
	if (he == NULL)
		return NULL;
	return (Tag *)Tcl_GetHashValue(he);
}

int NetModel::registerObjName(const char *name, int id)
{
	int newEntry = 1;
	Tcl_HashEntry *he = 
		Tcl_CreateHashEntry(objnameHash_, name, &newEntry);
	if (he == NULL)
		return (TCL_ERROR);
	if (newEntry) {
		Tcl_SetHashValue(he, (ClientData)id);
		nTag_++;
	}
	return (TCL_OK);
}

//XXX: maximum name length is 256. 
int NetModel::lookupObjname(const char *name)
{
#define STATIC_NAMELEN 256
	char n[STATIC_NAMELEN];
	size_t len = strlen(name);
	len = (len < STATIC_NAMELEN) ? len : STATIC_NAMELEN;
	for (size_t i = 0; i < len; i++)
		n[i] = toupper(name[i]);

	Tcl_HashEntry *he = Tcl_FindHashEntry(objnameHash_, n);
	if (he == NULL)
		return -1;
	return *Tcl_GetHashValue(he);
#undef STATIC_NAMELEN
}

/*
 * Unite tag name space and animation id space:
 * 
 * (1) if tag is a number, we'll first look it up in our 
 *     animation objects table. 
 * (2) if tag is a string, it must be in the tag table
 */
Animation* NetModel::lookupTagOrID(const char *name)
{
	char end[256];
	if (name == NULL)
		return NULL;
	unsigned int id = (unsigned int)strtoul(name, (char **)&end, 0);
	if (*end != 0)
		// This must be a tag string name
		return lookupTag(name);
	else 
		return Animation::find(id);
}

/*
 * Handling Tcl command "addtags" of EditView
 *
 * <view> addtag tag <searchSpec> arg...
 *
 * searchSpec can be: 
 *   all <ObjectType>
 *   closest x y ?halo? 
 *   enclosed x1 y1 x2 y2
 *   overlapping x1 y1 x2 y2
 *   withTag tagOrID
 * specification copied from Tk's canvas
 *
 * If newTag isn't null, then first lookup the tag. If found, add
 * the searchSpec results to that tag, otherwise create a new tag
 * and add results to that tag.
 * Return error if newTag == NULL.
 *
 * Tagging policies, e.g., whether an object can simultaneously belong 
 * to multiple tags, are left to the users.
 */
int NetModel::tagCmd(View *v, int argc, char **argv, 
		     char *newTag, char *cmdName)
{
	Tag *tag = NULL;
	size_t length;
	Tcl& tcl = Tcl::instance();
	int res = (TCL_OK), bNew = 0;

	if (newTag != NULL) {
		tag = (Tag *)lookupTagOrID(newTag);
		if ((tag != NULL) && (tag->classid() != ClassTagID)) {
			Tcl_AppendResult(tcl.interp(),
					 newTag, "should be a tag ",
					 (char *)NULL);
			return TCL_ERROR;
		}
		if (tag == NULL) {
			tag = new Tag(newTag);
			bNew = 1;
		}
	}

	int c = argv[0][0];
	length = strlen(argv[0]);

	if ((c == 'a') && (strncmp(argv[0], "all", length) == 0)
	    && (length >= 2)) {
		if (argc > 2) {
			Tcl_AppendResult(tcl.interp(), 
					 "wrong # args: should be \"", cmdName,
					 " all ?Obj?", (char *) NULL);
			res = TCL_ERROR;
			goto error;
		}
		Animation *a;
		int objType = ClassAllID;
		if (argc == 2) {
			// Map object name to class id
			objType = lookupObjname(argv[1]);
			if (objType == -1) {
				Tcl_AppendResult(tcl.interp(),
						 "Bad object name ", argv[1]);
				res = (TCL_ERROR);
				goto error;
			}
		}
		if (objType == ClassAllID) {
			for (a = animations_; a != NULL; a = a->next())
				if (!a->isTagged())
					tagObject(tag, a);
			for (a = drawables_; a != NULL; a = a->next())
				if (!a->isTagged())
					tagObject(tag, a);
		} else {
			for (a = animations_; a != NULL; a = a->next())
				if (!a->isTagged() && a->classid() == objType)
					tagObject(tag, a);
			for (a = drawables_; a != NULL; a = a->next())
				if (!a->isTagged() && a->classid() == objType)
					tagObject(tag, a);
		}

	} else if ((c == 'e') && (strncmp(argv[0], "enclosed", length) == 0)) {
		/* 
		 * enclosed x1 y1 x2 y2
		 */
		if (argc != 5) {
			Tcl_AppendResult(tcl.interp(), 
					 "wrong # args: should be \"",
					 cmdName, " enclosed x1 y1 x2 y2", 
					 (char *) NULL);
			res = TCL_ERROR;
			goto error;
		}
		BBox bb;
		// Translation/scaling will not change the order of bbox. :)
		if ((v->getCoord(argv[1], argv[2],
				 bb.xmin, bb.ymin) != TCL_OK) ||
		    (v->getCoord(argv[3], argv[4], bb.xmax, bb.ymax)!=TCL_OK))
			res = TCL_ERROR;
		else 
			res = tagArea(bb, tag, 1);

	} else if ((c == 'o') && 
		   (strncmp(argv[0], "overlapping", length) == 0)) {
		/*
		 * Overlapping x1 y1 x2 y2 
		 */
		if (argc != 5) {
			Tcl_AppendResult(tcl.interp(), 
					 "wrong # args: should be \"",
					 cmdName, " overlapping x1 y1 x2 y2",
					 (char *) NULL);
			res = TCL_ERROR;
			goto error;
		}
		BBox bb;
		// Translation/scaling will not change the order of bbox. :)
		if ((v->getCoord(argv[1], argv[2],
				 bb.xmin, bb.ymin) != TCL_OK) ||
		    (v->getCoord(argv[3], argv[4], bb.xmax, bb.ymax)!=TCL_OK))
			res = TCL_ERROR;
		else 
			res = tagArea(bb, tag, 0);

	} else if ((c == 'w') && (strncmp(argv[0], "withtag", length) == 0)) {
		if (argc != 2) {
			Tcl_AppendResult(tcl.interp(), 
					 "wrong # args: should be \"",
					 cmdName, " withtag tagOrId", 
					 (char *) NULL);
			res = TCL_ERROR;
			goto error;
		}
		// Find that tag or ID
		Animation *p = lookupTagOrID(argv[1]);
		if (p == NULL) {
			// Wrong tag
			Tcl_AppendResult(tcl.interp(),
					 "wrong tag ", argv[1],
					 (char *)NULL);
			res = TCL_ERROR;
		} else {
			// Label them as this tag
			tagObject(tag, p);
		}

	} else if ((c == 'c') && (strncmp(argv[0], "closest", length) == 0)) {
		float coords[2], halo;
		if ((argc < 3) || (argc > 5)) {
			Tcl_AppendResult(tcl.interp(), 
					 "wrong # args: should be \"",
					 cmdName, " closest x y ?halo? ",
					 (char *) NULL);
			res = TCL_ERROR;
			goto error;
		}
		if (v->getCoord(argv[1], argv[2], 
				coords[0], coords[1]) != TCL_OK) {
			Tcl_AppendResult(tcl.interp(),
					 "bad coordinates ",
					 argv[1], " ", argv[2], (char *)NULL);
			res = TCL_ERROR;
			goto error;
		}
		if (argc > 3) {
			// XXX: It's impossible to convert length in 
			// screen space to world space with translation
			// and scaling. We think it's in world space
			halo = strtod(argv[3], NULL);
			if (halo < 0.0) {
				Tcl_AppendResult(tcl.interp(), 
				       "can't have negative halo value \"",
						 argv[3], "\"", (char *) NULL);
				res = TCL_ERROR;
				goto error;
			}
		} else {
			halo = 0.0;
		}
		Animation *p = findClosest(coords[0], coords[1], halo);
		if (p != NULL)
			tagObject(tag, p);
		else 
			res = TCL_ERROR;

	} else  {
		Tcl_AppendResult(tcl.interp(), 
				 "bad search command \"", argv[0],
			 "\": must be above, all, below, closest, enclosed, ",
				 "overlapping, or withtag", (char *) NULL);
		res = TCL_ERROR;
		goto error;
	}

	if (res == TCL_OK) {
		if (bNew)
			add_tag(tag);
		return TCL_OK;
	}
error:
	if (tag != NULL)
		delete tag;
	return res;
}

Animation* NetModel::findClosest(float dx, float dy, double halo)
{
	double closestDist;
	Animation *startPtr, *closestPtr, *itemPtr;
	BBox bb;

	startPtr = animations_;
	itemPtr = startPtr;
	if (itemPtr == NULL) {
		return NULL;
	}
	closestDist = itemPtr->distance(dx, dy) - halo;

	if (closestDist < 0.0) {
		closestDist = 0.0;
	}
	while (1) {
		double newDist;
		/*
		 * Update the bounding box using itemPtr, which is the
		 * new closest item.
		 */
		bb.xmin = dx - closestDist - halo - 1,
		bb.ymin = dy - closestDist - halo - 1,
		bb.xmax = dx + closestDist + halo + 1,
		bb.ymax = dy + closestDist + halo + 1;
		closestPtr = itemPtr;
		/*
		 * Search for an item that beats the current closest 
		 * one.
		 * Work circularly through the canvas's item list until
		 * getting back to the starting item.
		 */
		while (1) {
			itemPtr = itemPtr->next();
			if (itemPtr == NULL) {
				if (startPtr == animations_) {
					startPtr = drawables_;
					itemPtr = startPtr;
				} else 
					return closestPtr;
			}
			if (itemPtr->isTagged()|| !itemPtr->bbox().overlap(bb))
				continue;
			newDist = itemPtr->distance(dx, dy)-halo;
			if (newDist < 0.0) {
				newDist = 0.0;
			}
			if (newDist <= closestDist) {
				closestDist = newDist;
				break;
			}
		}
	}
}

//----------------------------------------------------------------------
// void
// NetModel::tagObject(Tag *tag, Animation * animation_object)
//----------------------------------------------------------------------
void
NetModel::tagObject(Tag *tag, Animation * animation_object) {
	Tcl & tcl = Tcl::instance();

	if (tag == NULL) {
		char str[20];
		sprintf(str, "No tag for %d", animation_object->id());
		Tcl_AppendElement(tcl.interp(), str);
		return;
	}

	if (tag == animation_object)
		return;

	// add animation object to the tag group 
	tag->add(animation_object);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
int NetModel::tagArea(BBox &bb, Tag *tag, int bEnclosed)
{
	Animation *p;

	// Because the area is a rectangle, we only need to 
	// find out all objects whose bounding boxes are 
	// within/overlapping the given rectangle.
	if (bEnclosed == 0) {
		// overlapping and enclosed
		for (p = animations_; p != NULL; p = p->next()) 
			if (!p->isTagged() && p->bbox().overlap(bb) 
			    && (p != tag))
				tagObject(tag, p);
		for (p = drawables_; p != NULL; p = p->next())
			if (!p->isTagged() && p->bbox().overlap(bb)
			    && (p != tag))
				tagObject(tag, p);
	} else {
		// enclosed only
		for (p = animations_; p != NULL; p = p->next()) 
			if (!p->isTagged() && bb.inside(p->bbox())
			    && (p != tag))
				tagObject(tag, p);
		for (p = drawables_; p != NULL; p = p->next())
			if (!p->isTagged() && bb.inside(p->bbox())
			    && (p != tag))
				tagObject(tag, p);
	}
	return TCL_OK;
}

int NetModel::deleteTagCmd(char *tagName, char *tagDel)
{
	Tag *p;
	Animation *q = NULL;

	p = (Tag *)lookupTagOrID(tagName);
	if (tagDel != NULL)
		q = lookupTagOrID(tagDel);

	if (p == NULL) {
		Tcl_AppendResult(Tcl::instance().interp(),
				 "bad tag ", tagName, (char *)NULL);
		return (TCL_ERROR);
	}
	if ((p->classid() != ClassTagID) || (p == q) || (q == NULL)){
		// If given an ID, or given a group only
		// delete it and do nothing else
		delete p;
		return TCL_OK; 
	}
	// Now delete tag p from object q
	q->deleteTag(p);
	return TCL_OK;
}

// ---------------------------------------------------------------------
// void
// NetModel::render(EditView* view, BBox &bb)
//   - This is a partial render which does not seem to be complete.
//     Above here are several other render functions which redisplay 
//     the entire model.
//
// XXX: Partial redraw is only working for EditView! Not for other
// animation views for now. We need a method in each Animation object
// to decide their "dirtiness" to make this redraw work. :( And the
// computation of clipping box should be purely within NetModel but
// not fed in externally
// ---------------------------------------------------------------------
void
NetModel::render(EditView* view, BBox &bb) {
  Animation *a;

  for (a = drawables_; a != 0; a = a->next()) {
    if (!a->bbox().overlap(bb)) {
      // if outside of our views bounding box then skip 
      // this animation object
      continue;
    }
    a->draw(view, now_);
  }

  for (a = animations_; a != 0; a = a->next()) {
    if (!a->bbox().overlap(bb)) {
      // if outside of our views bounding box then skip 
      // this animation object
      continue;
    }
    a->draw(view, now_);
  }
}

void NetModel::moveNode(Node *n) {
	//fprintf(stderr, "Moving nodes can only be performed by NetModel.\n");
	for (Edge *e = n->links(); e != 0; e = e->next_) {
		e->unmark();
		placeEdge(e, n);
		Node *dst = e->neighbor();
		// Should place reverse edges too
		Edge *p = dst->find_edge(n->num());
		if (p != NULL) {
			p->unmark();
			placeEdge(p, dst);
		}
		dst->clear_routes();
		dst->place_all_routes();
	}
	for (Agent *a = n->agents(); a != NULL; a = a->next()) {
		a->mark(0), a->angle(NO_ANGLE);
		placeAgent(a, n);
	}
	// Relayout all routes
	n->clear_routes();
	n->place_all_routes();
}

