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
 *
 * @(#) $Header: /cvsroot/nsnam/nam-1/feature.h,v 1.4 2001/04/18 00:14:15 mehringe Exp $ (LBL)
 */

#ifndef nam_feature_h
#define nam_feature_h

class Queue;
class Edge;
class Agent;
class Node;
class View;

#include "animation.h"

#define TIMER_STOPPED 0
#define TIMER_UP 1
#define TIMER_DOWN 2

class Feature : public Animation {
    public:
	inline const char* name() const { return (varname_); }
	inline int num() const { return (nn_); }
	inline double size() const { return (size_); }
	virtual void size(double s);
	virtual void reset(double);
	inline double x() const { return (x_); }
	inline double y() const { return (y_); }
	virtual void place(double x, double y);
	void varname(const char* name, int anchor);
	inline int anchor() const { return (anchor_); }
	inline void anchor(int v) { anchor_ = v; }
	inline Feature *next() const { return next_; }
	inline void next(Feature * f) { next_ = f; }
	virtual void set_feature(char *) {};
	virtual void set_feature(double, int, double) {};
	int inside(double, float, float) const;
	const char* info() const;
	virtual void monitor(double now, char *result, int len);
	inline int marked() const { return (mark_); }
	inline void mark(int v) { mark_ = v; }

	Feature* next_;
    protected:
	Feature(Agent *a, const char* name);
	void update_bb();
	void drawlabel(View*) const;

	double size_;
	double x_, y_;
	char *varname_;
	Agent* agent_;
	int nn_;
	int anchor_;
	int mark_;
};

class ListFeature : public Feature {
 public:
	ListFeature(Agent *a, const char* name);
	void set_feature(char *);
	virtual void size(double s);
	virtual void draw(View*, double now);
	void monitor(double now, char *result, int len);
	virtual void place(double x, double y);
 private:
	char *value_;
        float x0_, y0_;
        float x1_, y1_;
};

class VariableFeature : public Feature {
 public:
	VariableFeature(Agent *a, const char* name);
	void set_feature(char *);
	virtual void size(double s);
	virtual void draw(View*, double now);
	void monitor(double now, char *result, int len);
	virtual void place(double x, double y);
 private:
	char *value_;
        float x0_, y0_;
        float x1_, y1_;
};

class TimerFeature : public Feature {
 public:
	TimerFeature(Agent *a, const char* name);
	void set_feature(double timer, int direction, double time_set);
	virtual void size(double s);
	virtual void draw(View*, double now);
	void monitor(double now, char *result, int len);
	virtual void place(double x, double y);
 private:
	double timer_;
	double time_set_;
	int direction_;
        float x0_, y0_;
        float x1_, y1_;
};

#endif
