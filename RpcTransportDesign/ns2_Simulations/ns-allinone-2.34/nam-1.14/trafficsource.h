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

#ifndef nam_trafficsource_h
#define nam_trafficsource_h


#include <tclcl.h>
#include "animation.h"

#define SOURCE 10
#define DESTINATION 20

class Agent;

class TimeElement {
	public:
		TimeElement(double time);
		~TimeElement();

		TimeElement * next_;
		double time_;
};

// This is a list to keep track of start and stop times
class TimeList {
	public:
		TimeList() {head_ = NULL;}
		TimeList(double time);
		~TimeList();

		void clear();
		TimeElement * head() {return head_;}

		void add(double time);
		void remove(double time);

		void setList(const char * list_string);

		void getListString(char* buffer, int buffer_size);
		bool isOn(double time);
		double lastStopTime();

	private:
		TimeElement * head_;
};

class TrafficSource : public Animation, public TclObject {
public:
	TrafficSource(const char* type, int id, double _size);
	TrafficSource(const char * name, double _size);
	void attachTo(Agent * agent);
	void removeFromAgent();
	inline int number() {return number_;}
	virtual int classid() const { return ClassTrafficSourceID; }
	inline const char * name() const { return (label_); }
	virtual void reset(double);

	inline double x() const {return x_;}
	inline double y() const {return y_;}
	inline double width() const {return width_;}
	inline double height() const {return height_;}

	void place();
	const char* info() const;

	void label(const char* name);

	virtual double distance(double x, double y) const;
	void color(const char* name);
	virtual void size(double s);
	inline double size() const { return (size_); }

	inline TrafficSource * next() const { return next_;}

	inline void next(TrafficSource * ts) {next_ = ts;}


	inline void windowInit(int size) { windowInit_ = size;}
	inline void window(int size) { window_ = size;}
	inline void maxcwnd(int size) { maxcwnd_ = size;}

	void tracevar(const char* var);

	inline void setInterval(float interval) {interval_ = interval;}
	inline void setMaxpkts(int maxpkts) {maxpkts_ = maxpkts;}
	inline void setBurstTime(int burst_time) {burst_time_ = burst_time;}
	inline void setIdleTime(int idle_time) {idle_time_ = idle_time;}
	inline void setRate(int rate) {rate_ = rate;}
	inline void setShape(double shape) {shape_ = shape;}

	double stopAt();

	const char* getname() const;

	int inside(double, float, float) const;
	virtual void update_bb();

	virtual void draw(View * view, double now);

	int writeNsDefinitionScript(FILE *file);
	int writeNsActionScript(FILE *file);
	int saveAsEnam(FILE *file);

	const char * property(); 

	void setWidth(double w) {width_ = w;}
	void setHeight(double h) {height_ = h;}

private:
	void setDefaults(); 

	void placeNextTo(TrafficSource * neighbor);	// To the right of my neighbor
	void placeOnAgent();	// On top of my Agent

public:
	TrafficSource * next_, * previous_;
	TrafficSource * editornetmodel_next_;	// Used by editornetmodel to track all traffic sources for property editing purposes
	Agent * agent_;
	int number_;

	int packet_size_;
	float interval_;

	TimeList timelist;

protected:
	void drawlabel(View * view) const;

	double size_;
	double width_;
	double height_;
	double x_, y_;
	double angle_;
	int anchor_;
	char * label_;
	int window_, windowInit_, maxcwnd_;
	char* color_;

	// Exponential
	int burst_time_;
	int idle_time_;
	int rate_;

	// Pareto
	double shape_;
	
	int maxpkts_;	// Used by FTP 
};



#endif
