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
 * @(#) $Header: /cvsroot/nsnam/nam-1/queue.cc,v 1.18 2001/04/18 00:14:16 mehringe Exp $ (LBL)
 */

#ifdef WIN32
#include <windows.h>
#endif
#include "netview.h"
#include "psview.h"
#include "queue.h"
#include "drop.h"
#include "monitor.h"
#include "sincos.h"

QueueItem::QueueItem(const PacketAttr& p, double tim, long offset) :
  Animation(tim, offset),
  qnext_(0) {
  pkt_ = p;
}

QueueItem::~QueueItem()
{
  if (monitor_!=NULL) {
    monitor_->delete_monitor_object(this);
  }
}

//----------------------------------------------------------------------
// void 
// QueueItem::draw(View* nv, double time )
//   - Draws a packet queue on a link next to the node  
//   - only draw things that are within the bounding
//     box... otherwise, we might hang the display :( -kfall
//
//   - note: this is an incomplete fix; there remains
//           a bug where the packets in the queue aren't erased
//           properly when this case triggers 8/8/99
//
//           I believe I fixed this bug. It was due to th bounding 
//           box for the view was not being calculated properly so
//           queues would be cut off before they reached their proper
//           height.   -John Mehringer
//----------------------------------------------------------------------
void 
QueueItem::draw(View* nv, double time ) {
  BBox bb;
  nv->BoundingBox(bb);

  if (bb.inside(bb_)) {
    nv->fill(px_, py_, 4, paint_);
    if (monitor_ != NULL)
        monitor_->draw(nv, px_[0],py_[0]);
  }
}

float QueueItem::distance(float x, float y) const 
{
  float cx, cy;
  cx = (bb_.xmin + bb_.xmax) / 2;
  cy = (bb_.ymin + bb_.ymax) / 2;
  return ((cx-x) * (cx-x) + (cy-y)*(cy-y));
}

//---------------------------------------------------------------------
// void
// QueueItem::position(float& x, float& y)
//   - Gets the position (left corner) of the queue item and puts
//     it into x and y. 
//---------------------------------------------------------------------
void
QueueItem::position(float& x, float& y) {
  // Need to find out where px_ and py_ are being set.
  // Look at locate and Queue relocate.
  x = px_[0];
  y = py_[0];
}

void QueueItem::update_bb()
{
  bb_.xmin = bb_.xmax = px_[0];
  bb_.ymin = bb_.ymax = py_[0];
  for (int i = 0; i < 4; i++) {
    if (px_[i] < bb_.xmin)
      bb_.xmin = px_[i];
    if (px_[i] > bb_.xmax)
      bb_.xmax = px_[i];
    if (py_[i] < bb_.ymin)
      bb_.ymin = py_[i];
    if (py_[i] < bb_.ymax)
      bb_.ymax = py_[i];
  }
}

//----------------------------------------------------------------------
// void 
// QueueItem::locate(float x, float y, float dx, float dy)
//   - Place a queue item with it's bottom left corner at x,y.
//   - dx, dy are the width and height of the diagonal of the diamond
//     drawn to represent the queue item.
//   - In a vertical queue dx is 0 due to the way the Queue::place()
//     calculates packet height and width.
//----------------------------------------------------------------------
void
QueueItem::locate(float x, float y, float dx, float dy) {
  // Bottom Left Corner
  px_[0] = x;
  py_[0] = y;

  // Bottom Right Corner 
  x += dy;
  y -= dx;
  px_[1] = x;
  py_[1] = y;

  // Top Right Corner
  x += dx;
  y += dy;
  px_[2] = x;
  py_[2] = y;

  // Top  Left Corner
  x -= dy;
  y += dx;
  px_[3] = x;
  py_[3] = y;

  update_bb();
}

const char* QueueItem::info() const
{
  static char text[128];
  sprintf(text, "%s %d: %s\n   %d bytes\n",
    pkt_.type, pkt_.id, pkt_.convid, pkt_.size);
  return (text);
}

const char* QueueItem::getname() const
{
  static char text[128];
  sprintf(text, "p");
  return (text);
}

void QueueItem::monitor(Monitor *m, double /*now*/, char *result, int /*len*/)
{
  monitor_=m;
  sprintf(result, "%s %d: %s\n   %d bytes\n",
                pkt_.type, pkt_.id, pkt_.convid, pkt_.size);
}

MonState *QueueItem::monitor_state()
{
  // return any state we wish the monitor to remember after we're gone
  MonState *ms=new MonState;
  ms->type=MON_PACKET;
  ms->pkt.id=pkt_.id;
  return ms;
}

Queue::Queue(float angle)
  : cnt_(0), psize_(0.), nb_(0), angle_(angle)
{
  head_ = 0;
  tail_ = &head_;
}

void Queue::place(double psize, double x, double y) {
  psize_ = psize;
  double qc, qs;
  SINCOSPI(angle_, &qs, &qc);

  dx_ = qc * psize_;
  dy_ = qs * psize_;

  px_ = 3 * dx_ / 4;
  py_ = 3 * dy_ / 4;

  qx_ = x + 2 * dx_;
  qy_ = y + 2 * dy_;
}

void Queue::relocate()
{
  float x = qx_, y = qy_;

  for (QueueItem *q = head_; q != 0; q = q->qnext_) {
    q->locate(x, y, px_, py_);
    x += dx_;
    y += dy_;
  }
}

QueueItem *Queue::remove(const PacketAttr& p, int& pos)
{
  QueueItem* q;
  pos = 0;
  for (QueueItem **pp = &head_; (q = *pp) != 0; pp = &q->qnext_) {
    ++pos;
    if (q->pkt_.id == p.id
        /* kfall && q->pkt_.attr == p.attr, see below */
        && q->pkt_.size == p.size
        && strcmp(q->pkt_.convid, p.convid) == 0
        && strcmp(q->pkt_.type, p.type)== 0) {
      --cnt_;
      nb_ -= q->pkt_.size;
      *pp = q->qnext_;
      if (*pp == 0)
        tail_ = pp;
      relocate();
      return (q);
    }
  }
  return (0);
  /* 
   * the removal of the attr comparison is needed
   * because when RED marks packets, the "-" records
   * have a different attrib than the corresponding "r" or
   * "+" records.  If one looks for matching attribs,
   * then all of the packets drained from the queue
   * that have ecn marks on them are never actually
   * erased, leaving curious packets laying around
   * in the queue even when the outgoing link is idle
   * -kfall 8/8/99
   */
}

void Queue::enque(QueueItem *q, int mode)
{
  /*add to the tail if time is running forwards, else add to the head*/
  if (mode==QUEUE_TAIL) {
    *tail_ = q;
    tail_ = &q->qnext_;
    q->qnext_ = 0;
    ++cnt_;
    nb_ += q->pkt_.size;
    relocate();
  } else if (mode==QUEUE_HEAD) {
    q->qnext_=head_;
    head_=q;
    ++cnt_;
    nb_ += q->pkt_.size;
    relocate();
  }
}

void Queue::reset(double /*now*/)
{
  while (head_ != 0) {
    QueueItem *n = head_->qnext_;
    delete head_;
    head_ = n;
  }
  head_ = 0;
  tail_ = &head_;
  cnt_ = 0;
  nb_ = 0;
}
