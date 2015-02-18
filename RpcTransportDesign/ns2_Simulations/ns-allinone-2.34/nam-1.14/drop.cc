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
 *  This product includes software developed by the Computer Systems
 *  Engineering Group at Lawrence Berkeley Laboratory.
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
 * @(#) $Header: /cvsroot/nsnam/nam-1/drop.cc,v 1.14 2005/01/24 19:55:14 haldar Exp $ (LBL)
 */

#ifdef WIN32
#include <windows.h>
#endif
#include "netview.h"
#include "drop.h"
#include "monitor.h"
//<zheng: +++>
#include "parser.h"
#include <math.h>
//</zheng: +++>

//----------------------------------------------------------------------
//----------------------------------------------------------------------
Drop::Drop(float cx, float cy, float b, float size, double now,
           long offset, const PacketAttr& p) :
  Animation(now, offset),
  x_(cx),
  y_(cy),
  bottom_(b),
  psize_(size),
  start_(now), 
  pkt_(p),
  rotation_(0) {
  curPos_ = CurPos(now);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
Drop::~Drop() {
  if (monitor_!=NULL) {
    monitor_->delete_monitor_object(this);
  }
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void Drop::draw(View * c, double now) {
  /* XXX nuke array */
  //<zheng: +++>
  float tx,ty,tx2,ty2,td;
  //</zheng: +++>
  float fx[4], fy[4];
  double yy = CurPos(now);

  if (rotation_ & 2) {
    double d = (0.75 * 0.7071067812) * psize_;
    //<zheng: +++>
    //not too large
    if (ParseTable::nam4wpan) {
	    tx = x_;
	    ty = yy;
	    c->imap(tx,ty);
	    tx2 = x_ + 1;
	    ty2 = yy;
	    c->imap(tx2,ty2);
	    td = (tx2 - tx) * (tx2 - tx) + (ty2 - ty) * (ty2 - ty);
	    td = pow(td, 0.5);
	    if (d > td)
	       d = td;
    }
    //</zheng: +++>
    fx[0] = x_;
    fy[0] = yy - d;
    fx[1] = x_ - d;
    fy[1] = yy;
    fx[2] = x_;
    fy[2] = yy + d;
    fx[3] = x_ + d;
    fy[3] = yy;
  } else {
    double d = (0.75 * 0.5) * psize_;
    //<zheng: +++>
    //not too large
    if (ParseTable::nam4wpan) {
	    tx = x_;
	    ty = yy;
	    c->imap(tx,ty);
	    tx2 = x_ + 1;
	    ty2 = yy;
	    c->imap(tx2,ty2);
	    td = (tx2 - tx) * (tx2 - tx) + (ty2 - ty) * (ty2 - ty);
	    td = pow(td, 0.5);
	    if (d > td)
	       d = td;
    }
    //</zheng: +++>
    fx[0] = x_ - d;
    fy[0] = yy - d;
    fx[1] = x_ - d;
    fy[1] = yy + d;
    fx[2] = x_ + d;
    fy[2] = yy + d;
    fx[3] = x_ + d;
    fy[3] = yy - d;
  }

  // Draw a square
  c->fill(fx, fy, 4, paint_);

  if (monitor_) {
    monitor_->draw(c, x_,yy);
  }
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void Drop::update(double now) {
  ++rotation_;
  if ((now < start_) ||
      (CurPos(now) < bottom_)) {
    delete this;
  } else {
    curPos_ = CurPos(now);
    update_bb();
  }
}

void Drop::update_bb()
{
  double d = (0.75 * 0.7071067812) * psize_;
  bb_.xmin = x_ - d, bb_.xmax = x_ + d;
  bb_.ymin = curPos_ - d, bb_.ymax = curPos_ + d;
}

void Drop::reset(double now)
{
  if (now < start_ || CurPos(now) < bottom_)
    delete this;
  else 
    curPos_ = CurPos(now);
}

int Drop::inside(double now, float px, float py) const
{
  //float minx, maxx, miny, maxy;
  double yy = CurPos(now);
  double d = (0.75 * 0.7071067812) * psize_;
  return (px >= x_ - d && px <= x_ + d && py >= yy - d && py <= yy + d);
}


//----------------------------------------------------------------------
// double
// Drop::CurPos(double now) const
//  - Calculates the current position of the packet based upon the start
//    time of the packet drop and the maximum drop time
//----------------------------------------------------------------------
double
Drop::CurPos(double now) const {
  // This calculation starts the drop at y_ and runs to bottom_ within the
  // timeframe of MAX_DROP_TIME.  At start_ + MAX_DROP_TIME the packet 
  // should be at bottom_.
  double drop_distance = (now - start_)*(bottom_ - y_)/MAX_DROP_TIME + y_;

//  if (bottom_ < -10) {
   //quick hack for wireless model
//   drop_distance = y_ - (now - start_) * (0 - bottom_);
//   fprintf(stderr, "Using Wireless drop distance.\n");
//  }
   return drop_distance;
}


const char* Drop::info() const
{
  static char text[128];
  sprintf(text, "%s %d: %s\n  dropped at %g\n  %d bytes",
    pkt_.type, pkt_.id, pkt_.convid, start_, pkt_.size);
  return (text);
}

const char* Drop::getname() const
{
  static char text[128];
  sprintf(text, "d");
  return (text);
}

void Drop::monitor(Monitor *m, double /*now*/, char *result, int /*len*/)
{
  monitor_=m;
  sprintf(result, "%s %d: %s\n  dropped at %g\n  %d bytes",
                pkt_.type, pkt_.id, pkt_.convid, start_, pkt_.size);
}

MonState *Drop::monitor_state()
{
  MonState *ms=new MonState;
  ms->type=MON_PACKET;
  ms->pkt.id=pkt_.id;
  return ms;
}
