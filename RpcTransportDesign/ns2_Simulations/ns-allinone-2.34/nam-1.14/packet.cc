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
 * @(#) $Header: /cvsroot/nsnam/nam-1/packet.cc,v 1.33 2005/01/24 23:30:41 haldar Exp $ (LBL)
 */

#ifdef WIN32
#include <windows.h>
#endif
#include "transform.h"
#include "view.h"
#include "netview.h"
#include "psview.h"
#include "edge.h"
#include "node.h"
#include "packet.h"
//<zheng: +++>
#include "parser.h"
#define MIN_RANGE_WPAN	3
//</zheng: +++>

int Packet::count_ = 0;

BPacket::BPacket(double  x, double y ,const PacketAttr& p , double now, 
		 long offset, int direction, double duration, double radius )
	: Animation(now, offset) 
{
	x_ = x ;
	y_ = y ;
	pkt_ = p ;
	if (direction == FORWARDS ) {
		//radius_ = MIN_RANGE ;							//zheng: ---
		radius_ = (ParseTable::nam4wpan)?MIN_RANGE_WPAN:MIN_RANGE ;		//zheng: +++
		start_ = now ;
		if (ParseTable::wpan_bradius < 0) 
			ParseTable::wpan_bradius = (int)radius;	//zheng: +++
	} else {
		radius_ = MAX_RANGE ; // BACKWARD
		if (ParseTable::nam4wpan)						//zheng: +++
		if (ParseTable::wpan_bradius > 0) radius_ = ParseTable::wpan_bradius;	//zheng: +++
		start_ = now - duration;
	}
	direction_ = direction;
	aType_ = BPACKET;
	duration_ = duration;
	max_radius_ = radius;
	if (ParseTable::nam4wpan)							//zheng: +++
	if (ParseTable::wpan_bradius > 0) max_radius_ = ParseTable::wpan_bradius;	//zheng: +++
}

void BPacket::draw(View* nv, double now) {
	nv->circle(x_, y_,radius_,paint_);
}

void BPacket::update(double now)
{
	curTime_ = now;
	update_bb();

	if (now < start_ || now > start_ + duration_)
		delete this;
}

void BPacket::update_bb()
{
	// radius_ starts at MIN_RANGE, moves to MAX_RANGE over duration_
	//radius_ = ((curTime_ - start_) / duration_) * (max_radius_-MIN_RANGE)													//zheng: ---
	//	+ MIN_RANGE;																			//zheng: ---
	radius_ = ((curTime_ - start_) / duration_) * (max_radius_-((ParseTable::nam4wpan)?MIN_RANGE_WPAN:MIN_RANGE)) + ((ParseTable::nam4wpan)?MIN_RANGE_WPAN:MIN_RANGE);	//zheng: +++
}


const char* BPacket::info() const
{
	static char text[128];
	sprintf(text, "%s %d: %s\n  Sent at %.6f\n  %d bytes\n",
		pkt_.type, pkt_.id, pkt_.convid, start_, pkt_.size);
	return (text);
}

const char* BPacket::getname() const
{
	static char text[128];
	sprintf(text, "p");
	return (text);
}

void BPacket::monitor(Monitor *m, double , char *result, int )
{
	if (((direction_ == FORWARDS) && (radius_ > MAX_RANGE)) ||
	//    ((direction_ == BACKWARDS) && (radius_ < MIN_RANGE)) ) {						//zheng: ---
	    ((direction_ == BACKWARDS) && (radius_ < ((ParseTable::nam4wpan)?MIN_RANGE_WPAN:MIN_RANGE))) ) {	//zheng: +++
		result[0] = '\0' ; 
		return ;
	}
	
	monitor_=m;
	sprintf(result, "%s %d: %s\n Sent at %.6f\n %d bytes",
		pkt_.type, pkt_.id, pkt_.convid, start_, pkt_.size);
}

MonState *BPacket::monitor_state()
{
	/*return any state we wish the monitor to remember after we're gone*/
	MonState * ms = new MonState;
	ms->type = MON_PACKET;
	ms->pkt.id = pkt_.id;
	return ms;
}

int BPacket::inside(float px, float py) const
{
	double dx =  ((double) px - x_ ) ;
	double dy =  ((double) py - y_ ) ;
	double d = sqrt(dx*dx + dy*dy) ;
	double dev = 1 ;
	
	if ((d <= (radius_ + dev)) && (d >= (radius_ - dev))) 
		return 1 ;
	else 
		return 0;
}





/*
 * Compute the start and end points of the packet in the one dimensional
 * time space [0, link delay].
 */
inline int Packet::EndPoints(double now, double& tail, double& head) const
{
	int doarrow;

	if (now < ta_) {
		head = now - ts_;
		doarrow = 1;
	} else {
		head = edge_->delay();
		doarrow = 0;
	}
	double t = now - tx_;
	tail = (t <= ts_) ? 0. : t - ts_;

	tail = tail * edge_->reallength() / edge_->delay();
	head = head * edge_->reallength() / edge_->delay();


	return (doarrow);
}

Packet::Packet(Edge *e, const PacketAttr& p, double s, double txtime,
	       long offset ) : Animation(s, offset)
{
	edge_ = e;
	e->AddPacket(this);
	pkt_ = p;
	ts_ = s;
	ta_ = s + e->delay();
	tx_ = txtime;
	arriving_ = 0;
	curTime_ = s;
	update_bb(); // Initial setup
	count_++;
}

Packet::~Packet()
{
  if (monitor_!=NULL) {
    monitor_->delete_monitor_object(this);
  }
  count_--;
}

float Packet::distance(float /*x*/, float /*y*/) const 
{
	// TODO
	return HUGE_VAL;
}

/*
 * Compute the unit-space points for the packet polygon.
 * Return number of points in polygon.
 */
int Packet::ComputePolygon(double now, float ax[5], float ay[5]) const
{
	double tail, head;
	int doarrow = EndPoints(now, tail, head);
	double deltap = head - tail;
	const Transform& m = edge_->transform();
	double height = edge_->PacketHeight();
	//<zheng: +++>
	//not too large
	if (ParseTable::nam4wpan) {
		float tx,ty,tx2,ty2,td;
		tx = head;
		ty = 0.2 * height;
		m.imap(tx,ty);
		tx2 = head;
		ty2 = 0.2 * height + 1;
		m.imap(tx2,ty2);
		td = (tx2 - tx) * (tx2 - tx) + (ty2 - ty) * (ty2 - ty);
		td = pow(td, 0.5);
		if (height > td)
			height = td;
	}
	//</zheng: +++>
	
	float bot = 0.2 * height;
	float top = 1.2 * height;
	float mid = 0.7 * height;
	/*XXX put some air between packet and link */
	bot += 0.5 * height;
	top += 0.5 * height;
	mid += 0.5 * height;
	

	if (doarrow && deltap >= height) {
		/* packet with arrowhead */
		m.map(tail, bot, ax[0], ay[0]);
		m.map(tail, top, ax[1], ay[1]);
		m.map(head - 0.75 * height, top, ax[2], ay[2]);
		m.map(head, mid, ax[3], ay[3]);
		m.map(head - 0.75 * height, bot, ax[4], ay[4]);
		return (5);
	} else {
		/* packet without arrowhead */
		m.map(tail, bot, ax[0], ay[0]);
		m.map(tail, top, ax[1], ay[1]);
		m.map(head, top, ax[2], ay[2]);
		m.map(head, bot, ax[3], ay[3]);
		return (4);
	}
}

// Assuming that curTime_ is set correctly. This can be guaranteed since
// the only way to adjust current time is through Packet::update().
void Packet::update_bb()
{
	int npts;
	float x[5], y[5];
	npts = ComputePolygon(curTime_, x, y);

	bb_.xmin = bb_.xmax = x[0];
	bb_.ymin = bb_.ymax = y[0];
	while (--npts > 0) {
		if (x[npts] < bb_.xmin)
			bb_.xmin = x[npts];
		if (x[npts] > bb_.xmax)
			bb_.xmax = x[npts];
		if (y[npts] < bb_.ymin)
			bb_.ymin = y[npts];
		if (y[npts] > bb_.ymax)
			bb_.ymax = y[npts];
	}
}

int Packet::inside(double now, float px, float py) const
{
	int npts;
	float x[5], y[5];
	BBox bb;

	if (now < ts_ || now > ta_ + tx_)
		return (0);

	npts = ComputePolygon(now, x, y);
	bb.xmin = bb.xmax = x[0];
	bb.ymin = bb.ymax = y[0];
	while (--npts > 0) {
		if (x[npts] < bb.xmin)
			bb.xmin = x[npts];
		if (x[npts] > bb.xmax)
			bb.xmax = x[npts];
		if (y[npts] < bb.ymin)
			bb.ymin = y[npts];
		if (y[npts] > bb.ymax)
			bb.ymax = y[npts];
	}

	return bb.inside(px, py);
}

const char* Packet::info() const
{
	static char text[128];
	sprintf(text, "%s %d: %s\n  Sent at %.6f\n  %d bytes\n",
		pkt_.type, pkt_.id, pkt_.convid, ts_, pkt_.size);
	return (text);
}

const char* Packet::gettype() const
{
	static char text[128];
	sprintf(text, "%s", pkt_.type);
	return (text);
}

const char* Packet::getfid() const
{
	static char text[128];
	sprintf(text, "%s", pkt_.convid);
	return (text);
}

const char* Packet::getesrc() const
{
	static char text[128];
	sprintf(text, "%d", pkt_.esrc);
	return (text);
}

const char* Packet::getedst() const
{
	static char text[128];
	sprintf(text, "%d", pkt_.edst);
	return (text);
}

const char* Packet::getname() const
{
	static char text[128];
	sprintf(text, "p");
	return (text);
}

void Packet::monitor(Monitor *m, double , char *result, int )
{
  monitor_=m;
  sprintf(result, "%s %d: %s\n Sent at %.6f\n %d bytes",
	  pkt_.type, pkt_.id, pkt_.convid, ts_, pkt_.size);
}

MonState *Packet::monitor_state()
{
  /*return any state we wish the monitor to remember after we're gone*/
  MonState *ms=new MonState;
  ms->type=MON_PACKET;
  ms->pkt.id=pkt_.id;
  return ms;
}

void Packet::RearrangePoints(View *v, int npts, float x[5], float y[5]) const
{
	x[4] = (int) x[0] + 1;
	v->map(x[2], y[2]);
	v->map(x[1], y[1]);
	x[2] = (int) x[1] + 1;
	if (npts == 5) {
		v->map(x[3], y[3]);
		x[3] = x[2];
		v->imap(x[3], y[3]);
	}
	v->imap(x[4], y[4]);
	v->imap(x[2], y[2]);
}	

void Packet::CheckPolygon(View *v, int npts, float ax[5], float ay[5]) const
{
	float x[5], y[5];

	memcpy((char *)x, (char *)ax, npts * sizeof(float));
	memcpy((char *)y, (char *)ay, npts * sizeof(float));
	v->map(x[4], y[4]);
	v->map(x[0], y[0]);
	if ((x[4] > x[0]) && ((int)x[4] - (int)x[0] < 1)) {
		RearrangePoints(v, npts, x, y);
		memcpy((char *)ax, (char *)x, npts * sizeof(float));
		memcpy((char *)ay, (char *)y, npts * sizeof(float));
	} else if ((x[0] > x[4]) && (x[0] - x[4] < 1)) {
		float tmp;
		tmp = x[0], x[0] = x[4], x[4] = tmp;
		tmp = x[1], x[1] = x[2], x[2] = tmp;
		RearrangePoints(v, npts, x, y);
		tmp = x[0], x[0] = x[4], x[4] = tmp;
		tmp = x[1], x[1] = x[2], x[2] = tmp;
		memcpy((char *)ax, (char *)x, npts * sizeof(float));
		memcpy((char *)ay, (char *)y, npts * sizeof(float));
	}
}

void Packet::draw(View* nv, double now) {
	/* XXX */
	if (now < ts_ || now > ta_ + tx_)
		return;

	float x[5], y[5];
	int npts;
	npts = ComputePolygon(now, x, y);
	//CheckPolygon(nv, npts, x, y);

	// Stupid way to decide fill/unfilled!
//  	if ((pkt_.attr & 0x100) == 0)
//  		nv->fill(x, y, npts, paint_);
//  	else 
//  		nv->polygon(x, y, npts, paint_);
	nv->fill(x, y, npts, paint_);
	/*XXX stupid way to get size!*/
	if (monitor_!=NULL)
	  monitor_->draw(nv, x[0], y[0]);
}
/*
void Packet::draw(PSView* nv, double now) const
{
	if (now < ts_ || now > ta_ + tx_)
		return;

	float x[5], y[5];
	int npts;
	npts = ComputePolygon(now, x, y);
	nv->fill(x, y, npts, paint_);
}
*/

void Packet::update(double now)
{
  if ((now > ta_)&&(arriving_==0))
    {
      /*If the packet has started to arrive, trigger any arrival event
	for the edge*/
      edge_->arrive_packet(this, ta_);
      arriving_=1;
    }
  if (now > ta_ + tx_ || now < ts_)
    {
		/* XXX this does not belong here */
#ifdef DEBUG
      printf("packet %d arrived from %d at %d\n", pkt_.id, edge_->src(), edge_->dst());
#endif
      /*remove this packet from the edge packet list*/
      edge_->DeletePacket(this);

      delete this;
    } else {
	    // Current time has changed, update its bounding box
	    // XXX No clean way to keep its bb_ up-to-date. :(
	    curTime_ = now;
	    update_bb();
    }
}

void Packet::position(float& x, float& y, double now) const
{
  float xs[5], ys[5];
  int npts;
  int i;
  /*XXX using ComputePolygon is overkill*/
  npts = ComputePolygon(now, xs, ys);
  x=0;y=0;
  for(i=0;i<npts;i++)
    {
      x+=xs[i];
      y+=ys[i];
    }
  x/=npts;
  y/=npts;
}
