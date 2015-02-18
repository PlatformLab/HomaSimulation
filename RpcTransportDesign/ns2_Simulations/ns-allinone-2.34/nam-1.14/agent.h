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
 * @(#) $Header: /cvsroot/nsnam/nam-1/agent.h,v 1.21 2005/02/07 20:47:41 haldar Exp $ (LBL)
 */

#ifndef nam_agent_h
#define nam_agent_h

#include <tclcl.h>

#include "animation.h"
#include "trafficsource.h"

class Queue;
class Edge;
class Feature;
class Node;
class NetView;
//class PSView;
class Monitor;

#define SOURCE 10
#define DESTINATION 20


class Agent : public Animation, public TclObject {
public:
  virtual int classid() const { return ClassAgentID; }
  inline const char* name() const { return (label_); }

  inline int num() const { return (number_); }
  inline int number() const { return (number_); }
  inline void setNumber(int number) {number_ = number;}

  inline double size() const { return (size_); }
  virtual void size(double s);
  virtual void reset(double);
  virtual double x() const { return (x_); }
  virtual  double y() const { return (y_); }
  inline double width()  {return width_;}
  inline double height() {return height_;}
  virtual void findClosestCornertoPoint(double x, double y, 
               double &corner_x, double &corner_y) const = 0; 
  virtual void place(double x, double y);
  void label(const char* name, int anchor);
  void color(const char* name);
  inline int anchor() const { return (anchor_); }
  inline void anchor(int v) { anchor_ = v; }
  inline Agent *next() const { return next_;}
  inline void next(Agent *a) { next_=a;}
  inline Edge *edge() const { return edge_;}
  inline double angle() const { return angle_;}
  inline void angle(double a) { angle_=a;}
  virtual float distance(float x, float y) const;

  void flowcolor(const char* color);
  inline void windowInit(int size) { windowInit_ = size;}
  inline void window(int size) { window_ = size;}
  inline void maxcwnd(int size) { maxcwnd_ = size;}
  void tracevar(const char* var);
  inline void startAt(float time) { start_ = time;}
  inline void stopAt(float time) { stop_ = time;}
  inline void produce(int number) { produce_ = number;}
  inline void packetSize(int size) { packetSize_ = size;}
  inline void interval(float size) { interval_ = size;}

  inline float startAt() { return start_;}
  inline float stopAt() { return stop_;}
  inline int produce() { return produce_;}

  TrafficSource * addTrafficSource(TrafficSource * ts);
  TrafficSource * removeTrafficSource(TrafficSource * ts);

  void add_feature(Feature *f);
  Feature *find_feature(char *) const;
  void delete_feature(Feature *f);
  const char* info() const;
  const char* getname() const;
  void monitor(Monitor *m, double now, char *result, int len);
  inline int marked() const { return (mark_); }
  inline void mark(int v) { mark_ = v; }
  virtual void update_bb();

  void showLink() {draw_link_ = true;}
  void hideLink() {draw_link_ = false;}

  int writeNsDefinitionScript(FILE * file);
  int writeNsConnectionScript(FILE * file);
  int saveAsEnam(FILE *file);
  const char* property();

  Agent * next_;
  Node* node_;
  int AgentRole_;
  Agent * AgentPartner_;
  TrafficSource * traffic_sources_;  // This list is modified by
                                     // add(remove)TrafficSource

  bool draw_link_;

protected:
  Agent(const char * type, int id, double _size);
  Agent(const char* name, double _size);

  void drawlabel(View*) const;

  int number_;

  double size_;
  double x_, y_;
  double width_;
  double height_;

  Feature* features_;
  Edge* edge_;
  double angle_;
  int anchor_;
  int mark_;
  char* label_;
  char * color_;

  // Following is used by Nam Editor to store data to 
  // be sent to ns
  int window_, windowInit_, maxcwnd_;
  int packetSize_;
  float interval_;
  char * tracevar_;
  char * flowcolor_;
  float start_, stop_;
  int produce_;
private:
  void setDefaults();

};

class BoxAgent : public Agent {
public:
  BoxAgent(const char * type, int id, double size);
  BoxAgent(const char* name, double size);
  virtual void size(double s);
  virtual void draw(View * nv, double time);
  //  virtual void draw(PSView*, double now) const;

  virtual void place(double x, double y);
  int inside(double, float, float) const;
  virtual void update_bb();
  virtual void findClosestCornertoPoint(double x, double y, double &corner_x,
                                double &corner_y) const; 
  void setWidth(double w) {width_ = w;}
  void setHeight(double h) {height_ = h;}

  virtual double x() const {return x0_;}
  virtual double y() const {return y0_;}

private:
  double x0_, y0_;
//  double x1_, y1_;
};

#endif
