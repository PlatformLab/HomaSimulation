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
 */
#include "sincos.h"
#include "config.h"
#include "lan.h"
#include "edge.h"
#include "packet.h"
#include "node.h"
#include "view.h"
#include "psview.h"
#include "paint.h"
#include "netmodel.h"
#include "trace.h"

LanLink::LanLink(Edge *e): edge_(e)
{
  /*need to search through the destination node's links to find the link
    that's the twin of this one (i.e, same nodes, other direction) */
  int lan=e->src();
  Node *n=e->neighbor();
  Edge *te=n->links();
  while (te!=NULL) {
    if (te->dst()==lan) {
      pair_=te;
      break;
    }
    te=te->next_;
  }
}

int LanLink::placed()
{
  return edge_->neighbor()->marked();
}

//----------------------------------------------------------------------
// Lan::Lan(const char *name, NetModel *nm, double ps, double bw,
//          double delay, double angle)
//----------------------------------------------------------------------
Lan::Lan(const char *name, NetModel *nm, double ps, double bw,
         double delay, double angle) :
  Animation(0,0),
  nm_(nm),
  links_(NULL),
  ps_(ps),
  bw_(bw), 
  delay_(delay),
  angle_(angle),
  marked_(0),
  max_(0) {

  virtual_node_ = new VirtualNode(name, this);
  name_ = new char[strlen(name) + 1];
  strcpy(name_, name);
  ln_ = atoi(name); /*XXX*/
  paint_ = Paint::instance()->thick();

  dropHash_ = new Tcl_HashTable;
  // XXX: unique packet id == (src, dst, id). Its size may be changed later.
  Tcl_InitHashTable(dropHash_, 3);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
Lan::~Lan() {
  if (dropHash_ != NULL) {
    Tcl_DeleteHashTable(dropHash_);
    delete dropHash_;
  }

  delete virtual_node_;
}

float Lan::distance(float /*x*/, float /*y*/) const 
{
	// TODO: to be added
	return HUGE_VAL;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
static int to_the_left(Edge *e, double angle, int incoming) {
  double edge_angle = e->angle();
  if (incoming)
    edge_angle = 1.0 + edge_angle;
  if (edge_angle > 2.0)
    edge_angle -= 2.0;

  double a = angle - edge_angle;

  if (a < 0) {
    a += 2.0;
  }

  if (a > 1.0) {
    return 1;
  } else {
    return 0;
  }
}


//----------------------------------------------------------------------
// void Lan::add_link(Edge *e)
//   - add a link to the shared bus line
//----------------------------------------------------------------------
void Lan::add_link(Edge *e) {
  int left = 0; 
  int right = 0;
  double sine, cosine, left_length, right_length;
  Node * source, * destination;
  LanLink * ll;

  ll = new LanLink(e);
  source = e->getSourceNode();
  destination = e->getDestinationNode();
  SINCOSPI(angle_, &sine, &cosine);

  ll->next(links_);
  links_ = ll;
  virtual_node_->add_link(e);

  // Keep track of how long the "bus" is
  ll = links_;
  while (ll != NULL) {
    if (to_the_left(ll->edge(), angle_, 0)) {
      left++;
    } else {
      right++;
    }
    ll = ll->next();
  }

  left_length = left * (destination->size() + source->size() + size_);
  right_length = right * (destination->size() + source->size() + size_);

  if (max_ < left_length) {
    max_ = (int) ceil(left_length);
   }

  if (max_ < right_length) {
    max_ = (int) ceil(right_length);
  }
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void Lan::update_bb() {
	double s,c;
	SINCOSPI(angle_,&s,&c);
	bb_.xmin = x_ - size_*c;
        bb_.xmax = x_+size_*c*(2*max_+1);
	bb_.ymin = y_ - size_*s;
        bb_.ymax = y_+size_*s*(2*max_+1);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
Lan::size(double size) {
  size_ = size;
  update_bb();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
Lan::place(double x, double y) {
  x_ = x;
  y_ = y;
  update_bb();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void Lan::draw(class View* nv, double time) {
  double s, c;
  SINCOSPI(angle_, &s, &c);
  //nv->line(x_ - size_ * c, y_ - size_ * s,
  //         x_ + size_ * c * (2 * max_ + 1), y_ + size_ * s * (2 * max_ + 1),
  //         paint_);
  nv->line(x_ - size_ * c, y_ - size_ * s,
           x_ + size_ * c + c * max_, y_ + size_ * s + s * max_,
           paint_);
}

//void Lan::draw(class PSView* nv, double /*time*/) const {
/*
  double s,c;
  SINCOSPI(angle_,&s,&c);
  nv->line(x_-size_*c, y_-size_*s, x_+size_*c*(2*max_+1), 
 	   y_+size_*s*(2*max_+1), paint_);
}
*/
void Lan::remove_drop(const TraceEvent &e)
{
  int id[3];
  id[0] = e.pe.src;
  id[1] = e.pe.dst;
  id[2] = e.pe.pkt.id;
  Tcl_HashEntry *he = Tcl_FindHashEntry(dropHash_, (const char *)id);
  if (he != NULL) {
    TraceEvent *pe = (TraceEvent *)Tcl_GetHashValue(he);
    delete pe;
    Tcl_DeleteHashEntry(he);
  }
}

void Lan::register_drop(const TraceEvent &e)
{
	int newEntry = 1;
	int id[3];
	id[0] = e.pe.src;
	id[1] = e.pe.dst;
	id[2] = e.pe.pkt.id;
	Tcl_HashEntry *he = Tcl_CreateHashEntry(dropHash_, (const char *)id, 
						&newEntry);
	if (he == NULL)
		return;
	if (newEntry) {
	  TraceEvent *pe = new TraceEvent;
	  *pe = e;
	  Tcl_SetHashValue(he, (ClientData)pe);
	}
}

void Lan::arrive_packet(Packet *p, Edge *e, double atime) {
  /*need to duplicate the packet on all other links 
    except the arrival link*/
  LanLink *l=links_;
  PacketAttr pkt;
  int id[3];

  pkt.size=p->size();
  pkt.id=p->id();
  pkt.attr=p->attr();
  strcpy(pkt.type,p->type());
  strcpy(pkt.convid,p->convid());
  while (l!=NULL) {
    Edge *ne=l->edge();
    if (l->pair()!=e) {
      //       Packet *np = nm_->newPacket(pkt, ne, atime);
      nm_->newPacket(pkt, ne, atime);

      id[0] = ne->src();
      id[1] = ne->dst();
      id[2] = p->id();
      Tcl_HashEntry *he = Tcl_FindHashEntry(dropHash_, (const char *)id);
      if (he != NULL) {
	// This is a drop packet, fake a trace event and add a drop
	TraceEvent *pe = (TraceEvent *)Tcl_GetHashValue(he);
	pe->time = atime;
	// The fact that this trace event is still there implies that 
	// we are going forwards.
	nm_->add_drop(*pe, atime, FORWARDS);
	delete pe;
	Tcl_DeleteHashEntry(he);
      }
    }
    l=l->next();
  }
  rgb *color = Paint::instance()->paint_to_rgb(p->paint());
  paint_ = Paint::instance()->lookup(color->colorname, 5);
}

void Lan::delete_packet(Packet *) {
  paint_ = Paint::instance()->thick();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
double Lan::x(Edge *e) const {
  double sine, cosine;
  SINCOSPI(angle_, &sine, &cosine);
  LanLink * l = links_;
  int incoming =- 1;
  int left = -1;
  int right = -1;
  Node * source, * destination;

  source = e->getSourceNode();
  destination = e->getDestinationNode();

  while (l != NULL) {
    if (to_the_left(l->edge(), angle_, 0)) {
      left++;
    } else {
      right++;
    }
    if (l->pair() == e) {
      incoming = 1;
      break;
    } else if (l->edge()==e) {
      incoming = 0;
      break;
    }
    l = l->next();
  }

  if (to_the_left(e, angle_, incoming)) {
    return x_ + cosine * left * (destination->size() + source->size() + size_);
    //return x_ + cosine * left * 2 * size_;
  } else {
    return x_ + cosine * right * (destination->size() + source->size() + size_);
    //return x_ + cosine * right * 2 * size_; 
  }
}

double Lan::y(Edge *e) const {
  double s,c;
  SINCOSPI(angle_,&s,&c);
  LanLink *l=links_;
  int incoming=-1;
  int left=-1;
  int right=-1;
  while (l!=NULL) {
    if (to_the_left(l->edge(), angle_,0)) {
      left++;
    } else {
      right++;
    }
    if (l->pair()==e) {
      incoming=1;
      break;
    } else if (l->edge()==e) {
      incoming=0;
      break;
    }
    l=l->next();
  }

  Node * destination = e->getDestinationNode();
  if (to_the_left(e, angle_, incoming)) {
    return y_ + s * left * destination->size();
  } else {
    return y_ + s * right * destination->size();
  }
}

#ifdef NOTDEF
Edge *Lan::lookupEdge(Node *n) {
  LanNode *ln=nodes_;
  while(ln!=NULL) {
    if (ln->node()==n)
    {
      return ln->e1_;
    }
  }
  return NULL;
}
#endif
