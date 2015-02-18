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
 * @(#) $Header: /cvsroot/nsnam/nam-1/edge.h,v 1.32 2003/10/11 22:56:49 xuanc Exp $ (LBL)
 */

#ifndef nam_edge_h
#define nam_edge_h

#include <math.h>
#include <tclcl.h>
#include "animation.h"
#include "transform.h"

class View;
//class PSView;
class Transform;
class Node;
class Packet;
class Monitor;
class LossModel;
class QueueHandle;

class Edge : public Animation, public TclObject {
public:
	Edge(double ps, double bw, double delay, double length, double angle);
	Edge(Node* src, Node* dst, double ps,
		double bw, double _delay, double length, double angle);
	Edge(Node* src, Node* dst, double ps,
	     double bw, double _delay, double length, double angle, int ws);
	~Edge();

	void attachNodes(Node * source, Node * destination);
	void addLossModel(LossModel * lossmodel);
	LossModel * getLossModel();
	void clearLossModel();

	void addQueueHandle(QueueHandle * q_handle);
	QueueHandle * getQueueHandle() {return queue_handle_;}

	virtual void draw(View*, double);
	virtual void reset(double);

	virtual int classid() const { return ClassEdgeID;}
	virtual float distance(float x, float y) const;

	inline double PacketHeight() const { return (psize_); }
	inline double size() const { return (psize_); }
	inline void size(double s) { psize_ = s; }

	inline int src() const { return (src_); }
	inline int dst() const { return (dst_); }
	Node * getSourceNode() {return start_;}
	Node * getDestinationNode() {return neighbor_;}

	inline int match(int s, int d) const { return (src_ == s && dst_ == d); }
	inline const Transform& transform() const { return (matrix_); }
	// bandwidth_ is converted to bits/sec for txtime calculation see tcl/netModel.tcl
	inline double txtime(int n) const {return (double(n << 3)/(bandwidth_*1000000.0));}
	inline double angle() const { return (angle_); }
	inline double realangle() const {
		return atan2(y1_-y0_, x1_-x0_);
	}
	inline void setAngle(double angle) { angle_ = angle; }
	double delay() const; // link delay is returned in seconds  
	inline double length() const { return (length_); }
	inline double x0() const { return (x0_); }
	inline double y0() const { return (y0_); }
	inline double x() const { return (x0_); }
	inline double y() const { return (y0_); }
	inline int anchor() const { return (anchor_); }
	inline void anchor(int v) { anchor_ = v; }
	inline int visible() const { return (visible_); }
	inline void visible(int v) { visible_ = v; }

	inline Node* neighbor() const { return (neighbor_); }
	inline Node* start() const { return (start_); }
	inline Packet* packets() const { return packets_;}
	inline double reallength() const { 
		return sqrt((x1_-x0_)*(x1_-x0_) + (y1_-y0_)*(y1_-y0_));
	}
	inline void setBW(double bw) { bandwidth_ = bw;}
	inline void setDelay(double dlay) { delay_ = dlay;}
	inline void setLength(double length) { length_ = length;}

	inline void inc_usage() { used_ = used_ + 1;}
	inline void dec_usage() { used_ = used_ - 1;}
	inline int used() const {return used_;}

	/*XXX for debugging only*/
	inline int no_of_packets() const {return no_of_packets_;}

	void init_color(const char *clr);
	void set_down(char *clr);
	void set_up();
	inline int isdown() const { return (state_==DOWN); }

	void place(double, double, double, double);
	virtual void update_bb() {} // Do nothing

	void AddPacket(Packet *);
	void DeletePacket(Packet *);
	void arrive_packet(Packet *, double atime);
	void dlabel(const char* name);
	void direction(const char* name);
	void dcolor(const char* name);

	virtual int inside(double, float, float) const;
	virtual int inside(float, float) const;
	const char* info() const;
	const char* property();
	const char* getname() const;
	void monitor(Monitor *m, double now, char *result, int len);
	inline int marked() const { return (marked_); }
	inline void mark() { marked_ = 1; }
	inline void unmark() { marked_ = 0; }
	int save(FILE *file);
	int saveAsEnam(FILE *file);
	int writeNsScript(FILE *file);

	Edge* next_;


private:
	void setupDefaults();

	int src_, dst_;
	Node* start_;
	Node* neighbor_;

	double x0_, y0_;
	double x1_, y1_;
	double psize_;		/* packet size */
	double angle_;
	double bandwidth_;	// link bandwidth (Mbits/sec) 
	double delay_;		// link delay in milliseconds
	double length_;		/* link length if not related to delay */
	Transform matrix_;	/* rotation matrix for packets */
	int state_;
	BBox eb_;	/* own bounding box in its own coordinate */
				/* system */
	
	Packet* packets_;       /*newest packet to be added*/
	int no_of_packets_;
	Packet* last_packet_;   /*oldest packet - normally first to arrive*/

	int marked_;
	int visible_;

	char* dlabel_;          /* Label to be drawn beneath the link */
	char* dcolor_;          /* label color */
	int direction_;
	int wireless_;
	int anchor_;
	int used_ ;             /* is the edge currently being used */

	LossModel * lossmodel_;
	QueueHandle * queue_handle_;
};


#endif





