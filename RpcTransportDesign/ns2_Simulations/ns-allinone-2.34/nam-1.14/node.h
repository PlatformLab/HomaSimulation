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
 * @(#) $Header: /cvsroot/nsnam/nam-1/node.h,v 1.40 2006/09/28 06:25:03 tom_henderson Exp $ (LBL)
 */

#ifndef nam_node_h
#define nam_node_h

class Queue;
class Edge;
class Agent;
class Route;
class Monitor;
class NetView;
//class PSView;
class EditView;
class Lan;

#include <math.h>
#include <string.h>
#include <tcl.h>
#include <tclcl.h>

#include "animation.h"

struct NodeMark {
	NodeMark() { name = NULL; color = 0; next = 0; }
	NodeMark(char *str) {
		name = new char[strlen(str)+1];
		strcpy(name, str);
		color = 0;
		next = 0;
	}
	~NodeMark() { delete []name; }

	char *name;
	int color;
	int shape;
	struct NodeMark *next;
};

const float NodeMarkScale = 0.15;
const int NodeMarkCircle = 0;
const int NodeMarkSquare = 4;
const int NodeMarkHexagon = 6;

class MovementElement {
public:
	MovementElement(double time, double x, double y);
	MovementElement(double time, double x, double y,
	                double x_velocity, double y_velocity, double duration);
	~MovementElement();

	bool contains(double time, double duration);  // returns true if time >= time_ 
	                                              // and < time_ + duration_

	MovementElement * next_;
	double time_;
	double x_, y_;
	double x_velocity_, y_velocity_;
	double duration_;
};

// This is a list to keep track of node movements
class MovementList {
public:
	MovementList();
	~MovementList();

	void clear();
	MovementElement * head() {return head_;}

	MovementElement * add(double x, double y, double time);
	void remove(double time);

	void setList(char * list_string);

	int getListString(char* buffer, int buffer_size);
	bool isMoving(double time);
	double lastStopMovement();

	void getPositionAt(double time, double & x, double & y);

private:
	MovementElement * head_;
	int size_;
};




class Node : public Animation, public TclObject {
public:
	virtual ~Node();

	virtual int classid() const { return ClassNodeID; }
	inline const char* name() const { return (label_); }
	inline char* ncolor() const { return (ncolor_); }
	inline int num() const { return (nn_); }
	inline int number() const { return (nn_); }
	inline int addr() const { return (addr_); }
	inline double size() const { return (size_); }
	inline float nsize() const { return (nsize_); }
	inline Agent* agents() const { return (agents_); }
	virtual void size(double s);
	virtual void reset(double);
	inline double x() const { return (x_); }
	inline double y() const { return (y_); }
	virtual double x(Edge *) const { return (x_); }
	virtual double y(Edge *) const { return (y_); }
	inline double dx() const { return (dx_); }
	inline double dy() const { return (dy_); }
	void displace(double dx, double dy) {
		dx_ = dx; dy_ = dy;
	}
	virtual void move(EditView *v, float dwx, float dwy);
	void updatePositionAt(double time);
	virtual float distance(float x, float y) const;
	inline double distance(Node &n) const { 
		return sqrt((x_-n.x_)*(x_-n.x_) + (y_-n.y_)*(y_-n.y_));
	}
	inline double angle(Node &n) const {
		return atan2(n.y_-y_, n.x_-x_);
	}

	void setstart(double t) { starttime_ = t;}
	void setend(double t) { endtime_ = t;}
	void setvelocity(double x, double y) { x_vel_=x; y_vel_=y;}
	void placeorig(double x, double y) { xorig_=x; yorig_=y;}

	MovementElement * addMovementDestination(double world_x, 
	                                         double world_y,
	                                         double time);
	void removeMovementDestination(double time);
	int getMovementTimeList(char * buffer, int buffer_size);
	double getMaximumX();
	double getMaximumY();

	void setaddr(int addr) { addr_ = addr; }
	virtual void place(double x, double y);
	void label(const char* name, int anchor);
	void dlabel(const char* name);
	void lcolor(const char* name);
	void dcolor(const char* name);
	void ncolor(const char* name);
	void direction(const char* name);
	inline int anchor() const { return (anchor_); }
	inline void anchor(int v) { anchor_ = v; }
	void add_link(Edge*);
	void delete_link(Edge*);
	void add_agent(Agent*);
	void add_route(Route*);
	void delete_route(Route*);
	void place_route(Route*);
	void place_all_routes();
	void clear_routes();
	void delete_agent(Agent*);
	Route *find_route(Edge *e, int group, int pktsrc, int oif) const;
	Agent *find_agent(char *name) const;
	Edge *find_edge(int dst) const;
	int no_of_routes(Edge *e) const;
	virtual int inside(double t, float px, float py) const { 
		return Animation::inside(t, px, py);
	}
	const char* info() const;
	const char* property();
	const char* getname() const;
	void monitor(Monitor *m, double now, char *result, int len);
	inline Edge* links() const { return (links_); }
	inline int marked() const { return (mark_); }
	inline void mark(int v) { mark_ = v; }
	inline int mass() const { return mass_; }
	inline void mass(int m) {mass_=m;}
	virtual char *style() {return 0;}
	virtual void update(double now);

	void init_color(const char *clr);
	void set_down(char *color);
	void set_up();
	inline int isdown() const { return (state_ == DOWN); }

	NodeMark* find_mark(char *name);
	int add_mark(char *name, char *color, char *shape);
	void delete_mark(char *name);
	virtual void arrive_packet(Packet *, Edge *, double) {/*do nothing*/}
	virtual void delete_packet(Packet *) {/*do nothing*/}
	int save(FILE *file);
	int writeNsScript(FILE *file);
	int writeNsMovement(FILE *file);

	Node* next_;
	Queue *queue_;
	Queue* queue() { return queue_; }
	void add_sess_queue(unsigned int grp, Queue *q);
	char * getTclScript();
	char * getTclScriptLabel();
	void setTclScript(const char * label, const char * script);

	int command(int argc, const char * const * argv);

protected:
	Node(const char* name, double size);
	void update_bb();
	void drawlabel(View*) const;

	int mass_; //mass used in AutoNetModel

	double size_;
	float nsize_;
	double x_, y_;
	double xorig_, yorig_;
	double x_vel_,y_vel_;
	double starttime_, endtime_;
	Edge* links_;
	Route* routes_;
	Agent* agents_;
	int anchor_;
	int mark_;
	char* label_;
	int nn_;	// Node id
	int addr_;	// Node address
	int state_;

	NodeMark *nm_;
	int nMark_;
	void draw_mark(View *) const;
	char* dlabel_;  // Label to be drawn beneath the node
	char* lcolor_;  // Inside label color
	char* dcolor_;  // label color
	char* ncolor_;  // node color
	int direction_;

	double dx_, dy_; // displacements used in AutoNetModel

	char * tcl_script_label_; // Text for tcl_script info button
	char * tcl_script_; // Tcl script to be executed when 
	                    // tcl_script info button is pressed
	MovementList movement_list;

public:
	bool wireless_; // Indicates if this is a wireless node or not;
};

class BoxNode : public Node {
public:
	BoxNode(const char* name, double size);
	virtual void size(double s);
	virtual void draw(View*, double now);
	virtual void place(double x, double y);
	char *style() {return "box";}
private:
	float x0_, y0_;
	float x1_, y1_;
};

class CircleNode : public Node {
public:
	CircleNode(const char* name, double size);
	virtual void size(double s);
	virtual void draw(View*, double now);
	char *style() {return "circle";}
private:
	float radius_;
};

class HexagonNode : public Node {
public:
	HexagonNode(const char* name, double size);
	virtual void size(double s);
	virtual void draw(View*, double now);
	virtual void place(double x, double y);
	char *style() {return "hexagon";}
private:
	float ypoly_[6];
	float xpoly_[6];
};

class VirtualNode : public Node {
public:
	VirtualNode(const char* name, Lan *lan);
	virtual void size(double s);
	virtual void draw(View*, double now);
	virtual void place(double x, double y);
	virtual void arrive_packet(Packet *p, Edge *e, double atime);
	virtual void delete_packet(Packet *p);
	virtual double x(Edge *e) const;
	virtual double y(Edge *e) const;
	char *style() {return "virtual";}
private:
	Lan *lan_;
};

#endif
