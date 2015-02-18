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
 * @(#) $Header: /cvsroot/nsnam/nam-1/packet.h,v 1.20 2003/01/28 03:22:38 buchheim Exp $ (LBL)
 */

#ifndef nam_packet_h
#define nam_packet_h

#include "animation.h"
#include "monitor.h"
#include "trace.h"
#include "view.h" 

#define AGT 1
#define RTR 2
#define MAC 3
#define MAX_RANGE 250
#define MIN_RANGE 50
#define BPACKET_TXT_TIME 0.5


static const int PacketTypeSize = 8;

class NetView;
//class PSView;
class Edge;

class Packet : public Animation {
 public:
	Packet(Edge*, const PacketAttr&, double, double, long );
	~Packet();
	virtual float distance(float, float) const;
	void position(float&, float&, double) const;
	virtual void draw(View* nv, double now);
  //	virtual void draw(PSView*, double now) const;
	virtual void update(double now);
	int ComputePolygon(double now, float ax[5], float ay[5]) const;
	virtual int inside(double now, float px, float py) const;
	virtual const char* info() const;
	virtual const char* getname() const;
	virtual const char* getfid() const;
	virtual const char* getesrc() const;
	virtual const char* getedst() const;
	virtual const char* gettype() const;
	void monitor(Monitor *m, double now, char *result, int len);
	MonState *monitor_state();
	inline int id() const { return pkt_.id; }
	inline int size() const { return pkt_.size; }
	inline Packet* next() const {return next_;}
	inline void next(Packet *p) {next_=p;}
	inline Packet* prev() const {return prev_;}
	inline void prev(Packet *p) {prev_=p;}
	inline double start() const { return ts_; }
	inline double finish() const { return ta_ + tx_; }
	inline const int attr() const { return pkt_.attr; }
	inline const char* type() const { return pkt_.type; }
	inline const char* convid() const { return pkt_.convid; }

	virtual int classid() const { return ClassPacketID; }
	virtual void update_bb();

	void CheckPolygon(View *, int, float x[5], float y[5]) const;
	void RearrangePoints(View *v, int npts, float x[5], float y[5]) const;

	static int count() {return count_;}

 protected:
	virtual int EndPoints(double, double&, double&) const;

	Edge* edge_;
	Packet *prev_;          /*maintain a doubly linked list of */
	Packet *next_;          /*packets on a particular edge*/
	PacketAttr pkt_;
	double ts_;		/* time of start of transmission */
	double ta_;		/* time of arrival of leading edge */
	int arriving_;          /*has the packet started to arrive yet?*/
	double tx_;		/* tranmission time of packet on cur link */
	double curTime_;
	static int count_;
	int type_ ;
};

class BPacket : public Animation {
public:
   BPacket(double cx, double cy , const PacketAttr&, double now, long offset, int direction, double duration, double radius );
   virtual void draw(View* nv, double now);
   virtual void update(double);
   virtual void update_bb()  ;
   virtual const char* getname() const;
   virtual const char* info() const;
   virtual int inside(float px, float py) const;
   void monitor(Monitor *m, double now, char *result, int len);
   MonState *monitor_state();
private:
   double x_ ;
   double y_ ;
   PacketAttr pkt_;
   float radius_;
   double start_;          /* time packet was dropped */
   int direction_ ;
   double curTime_;
   double duration_;
   double max_radius_;

};

#endif
