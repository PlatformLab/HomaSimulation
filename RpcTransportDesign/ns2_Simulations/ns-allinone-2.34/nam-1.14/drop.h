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
 * @(#) $Header: /cvsroot/nsnam/nam-1/drop.h,v 1.9 2001/04/18 00:14:15 mehringe Exp $ (LBL)
 */

#ifndef nam_drop_h
#define nam_drop_h

#include "animation.h"

#define MAX_DROP_TIME 0.1 // Maxiumum amount of time a packet should 
                          // be animated droping off the screen in seconds

class View;
class Monitor;

// Used to show the animation of a packet drop
class Drop : public Animation {
public:
  Drop(float x, float y, float bot, float sz, double now,
       long offset, const PacketAttr&);
  ~Drop();
  virtual void draw(View*, double);
  virtual void update(double);
  virtual void update_bb();
  virtual void reset(double);
  virtual int inside(double now, float px, float py) const;
  virtual const char* info() const;
  virtual const char* getname() const;
  void monitor(Monitor *m, double now, char *result, int len);
  MonState *monitor_state();
private:
  inline double CurPos(double now) const;
  float x_, y_, bottom_;  // initial position of dropped packet
  float psize_;           // size of drawable object (a side)
  double start_;          // time packet was dropped 
  PacketAttr pkt_;
  int rotation_;          // rotation state (not critical)
  float curPos_;
};
#endif
