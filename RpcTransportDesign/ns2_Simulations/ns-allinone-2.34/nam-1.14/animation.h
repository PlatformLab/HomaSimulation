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
 * @(#) $Header: /cvsroot/nsnam/nam-1/animation.h,v 1.27 2003/10/11 22:56:49 xuanc Exp $ (LBL)
 */

#ifndef nam_animation_h
#define nam_animation_h

#include <tcl.h>
#include "state.h"
#include "paint.h"
#include "bbox.h"

class View;
//class PSView;
class EditView;
class Monitor;
struct MonState;

#define UP 0
#define DOWN 1
#define BPACKET 1

// Assign an ID for all derived classes of Animation
const int ClassAllID = 		0;
const int ClassAnimationID = 	1;
const int ClassPacketID = 	ClassAnimationID + 1;
const int ClassNodeID = 	ClassPacketID + 1;
const int ClassEdgeID = 	ClassNodeID + 1;
const int ClassQueueItemID = 	ClassEdgeID + 1;
const int ClassAgentID = 	ClassQueueItemID + 1;
const int ClassLanID = 		ClassAgentID + 1;
const int ClassTagID = 		ClassLanID + 1;
const int ClassTrafficSourceID = ClassTagID + 1;
const int ClassLossModelID = ClassTrafficSourceID + 1;
const int ClassQueueHandleID = ClassLossModelID + 1;

const int AnimationTagIncrement = 5;

class Animation {
public:
	virtual ~Animation();
	virtual void draw(View*, double now) = 0;

	// This one is obsolete. Need not pass "now" if everything is 
	// updated timely.
	virtual int inside(double now, float px, float py) const;
	virtual int inside(float px, float py) const {
		return bb_.inside(px, py);
	}

	// Move this object by a displacement in window coordinates
	virtual void move(EditView* editview,
	                  float x_displacement,
	                  float y_displacement) {}

	virtual float distance(float x, float y) const;

	void merge(BBox & b);

	virtual void update_bb() = 0;
	virtual void update(double now);
	virtual void reset(double now);

	virtual const char* info() const;
	virtual const char* property();
	virtual const char* getProperties(const char * type);
	virtual const char* getname() const;
	virtual const char* getfid() const;
	virtual const char* getesrc() const;
	virtual const char* getedst() const;
	virtual const char* gettype() const;
	int id() const { return id_; }
	const BBox& bbox() { return bb_; }
	virtual void monitor(Monitor *m, double now, char *result, int len);
	virtual MonState *monitor_state(void);
	void remove_monitor() {monitor_=0;}
	void insert(Animation **);
	void detach();
	void paint(int id) { paint_ = id; }
	void color(const char *color);
	void change_color(char *color);
	void toggle_color();

	inline StateInfo stateInfo() { return (si_); }
	inline int paint() const { return (paint_); }
	inline Animation* next() const { return (next_); }
	inline Animation** prev() const { return (prev_); }

	static unsigned int LASTID_;
	static Tcl_HashTable* AniHash_;
	static unsigned int nAniHash_;
	static Animation* find(unsigned int id);
	virtual int classid() const { return ClassAnimationID; }

	inline int isTagged() const { return nTag_ > 0; }
	inline int numTag() const { return nTag_; }
	inline int type() const { return aType_; }
	inline Animation* getTag(int i) const { return tags_[i]; }
	Animation* getLastTag();
	void deleteTag(Animation *);
	void addTag(Animation *);	// label myself with a new tag 
					// i.e., join a new group
protected:
	Animation();
	Animation(double, long);

	/*
	 * id for X graphics context
	 * (e.g., color and linewidth)
	 */
	int paint_;
	int oldPaint_; 		// saved for node down event
	Monitor *monitor_;
	BBox bb_;
	
	StateInfo si_;
	Animation* next_;
	Animation** prev_;

	unsigned int id_; 	// unique identifier for all animation objs

	Animation **tags_;	// an dynamic array of tags
	int nTag_;
	int aType_;
};

#endif
