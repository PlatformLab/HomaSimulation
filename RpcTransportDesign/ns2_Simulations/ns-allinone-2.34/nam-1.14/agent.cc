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
 * @(#) $Header: /cvsroot/nsnam/nam-1/agent.cc,v 1.39 2001/08/10 01:45:46 mehringe Exp $ (LBL)
 */

#include <stdlib.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "netview.h"
#include "psview.h"
#include "node.h"
#include "feature.h"
#include "agent.h"
#include "edge.h"
#include "paint.h"
#include "monitor.h"
#include "sincos.h"

static int g_agent_count = 1;  // Used to give unique id's to each agent

//----------------------------------------------------------------------
//  Wrapper for tcl creation of Agents 
//----------------------------------------------------------------------
static class AgentClass : public TclClass {
public:
  AgentClass() : TclClass("Agent") {} 

  TclObject * create(int argc, const char*const* argv) {
    Agent * agent;
    const char * type;
    int id;
    double size;

    type = argv[4];
    id  = strtol(argv[5], NULL, 10);
    size  = strtod(argv[6], NULL);

    agent = new BoxAgent(type, id, size);
    return agent;
  }
} class_agent;

//----------------------------------------------------------------------
//----------------------------------------------------------------------
Agent::Agent(const char * type, int id, double _size) :
  Animation(0, 0) {

  setDefaults();
  size(_size);

  label_ = new char[strlen(type) + 1];
  strcpy(label_, type);

  number_ = id;

  // Make sure new agents will have unique ids
  if (g_agent_count <= id) {
    g_agent_count = id + 1;
  }
}

//----------------------------------------------------------------------
// Agent::Agent(const char* name, double size)
//----------------------------------------------------------------------
Agent::Agent(const char* name, double _size) :
  Animation(0, 0),
  next_(0),
  node_(0),
  x_(0.0),
  y_(0.0),
  features_(NULL),
  edge_(NULL),
  angle_(NO_ANGLE),
  anchor_(0),
  mark_(0),
  color_(0),
  window_(20),
  windowInit_(1),
  maxcwnd_(0),
  packetSize_(210),
  interval_(0.0375),
  tracevar_(0),
  start_(0.0),
  stop_(10.0),
  produce_(0)
{
  size(_size);
  AgentPartner_ = NULL;
  flowcolor_ = NULL;
  flowcolor("black");

  number_ = g_agent_count;
  g_agent_count++;
  traffic_sources_ = NULL;
  label_ = new char[strlen(name) + 1];
  strcpy(label_, name);
  paint_ = Paint::instance()->thin();

  setDefaults();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
Agent::setDefaults() {
  next_ = NULL;
  node_ = NULL;
  AgentPartner_ = NULL;
  x_ = 0.0;
  y_ = 0.0;
  features_ = NULL;
  edge_ = NULL;
  angle_ = NO_ANGLE;
  anchor_ = 0;
  mark_ = 0;
  color_ = NULL;

  window_ = 20;
  windowInit_ = 1;
  maxcwnd_ = 0;
  packetSize_ = 210;
  interval_ = 0.0375;
  tracevar_ = NULL;
  start_ = 0.0;
  stop_  = 1.0;
  produce_ = 0;

  flowcolor_ = NULL;
  flowcolor("black");

  traffic_sources_ = NULL;
  paint_ = Paint::instance()->thin();
  showLink();
}

float Agent::distance(float x, float y) const {
	return ((x_-x)*(x_-x) + (y_-y)*(y_-y));
}

void Agent::color(const char* name)
{
     if (name[0] == 0) {
          if (color_) {
	       delete []color_;
	       color_ = 0;
	  }
	  return;
     }

     if (color_)
       delete []color_;
     color_ = new char[strlen(name) + 1];
     strcpy(color_, name);
}

void Agent::size(double s)
{
	size_ = s;
  width_ = s;
  height_ = s;
	update_bb();
}

void Agent::update_bb() {
	double off = 0.5 * size_;
	/*XXX*/
	bb_.xmin = x_ - off;
	bb_.ymin = y_ - off;
	bb_.xmax = x_ + off;
	bb_.ymax = y_ + off;
}

//----------------------------------------------------------------------
// TrafficSource * 
// TrafficSource::addTrafficSource(TrafficSource * ts)
//   - Add traffic source to the trafficsources_ list
//   - Returns the traffic source before the one just added
//----------------------------------------------------------------------
TrafficSource * 
Agent::addTrafficSource(TrafficSource * ts) {
  TrafficSource * run, * last;

  run = traffic_sources_;
  last = NULL;

  // Find last traffic source on list
  while (run) {
    last = run;
    run = run->next_;
  }

  // if it exists then add it to the list
  if (last) {
    last->next_ = ts;
  } else {
    traffic_sources_ = ts;
  }  
  return last;
}

//----------------------------------------------------------------------
// TrafficSource *
// Agent::removeTrafficSource(TrafficSource * ts) {
//----------------------------------------------------------------------
TrafficSource *
Agent::removeTrafficSource(TrafficSource * ts) {
  // ts->previous_ should be NULL for the first traffic source
  // attached to this agent
  if (ts->previous_) {
    ts->previous_->next_ = ts->next_;
  } else {
    // I am the first traffic source so
    // make agent point to my next_
    traffic_sources_ = ts->next_;
  }

  if (ts->next_) {
    ts->next_->previous_ = ts->previous_;
  }

  return ts;
}

void Agent::add_feature(Feature* f) {
       f->next_ = features_;
       features_ = f;
}

Feature *Agent::find_feature(char *name) const
{
  /*given a feature, remove it from an agent's feature list*/
  Feature *ft;
  ft=features_;
  while (ft!=NULL)
    {
      if (strcmp(ft->name(), name)==0)
	return ft;
      ft=ft->next();
    }
  return NULL;
}
 

 
void Agent::delete_feature(Feature* r)
{
  /*given a feature, remove it from an agent's feature list*/
  Feature *f1, *f2;
  f1=features_;
  f2=features_;
  while ((f1!=r)&&(f1!=NULL))
    {
      f2=f1;
      f1=f1->next();
    }
  if (f1==r)
    {
      f2->next(f1->next());
      if (f1==features_)
	features_=f1->next();
    }
}
 

 
void Agent::label(const char* name, int anchor)
{
	delete label_;
	label_ = new char[strlen(name) + 1];
	strcpy(label_, name);
	anchor_ = anchor;
}

void Agent::drawlabel(View* nv) const
{
	/*XXX node number */
	if (label_ != 0)
		nv->string(x_, y_, size_, label_, anchor_);
}
/*
void Agent::drawlabel(PSView* nv) const
{
	if (label_ != 0)
		nv->string(x_, y_, size_, label_, anchor_);
}
*/

//----------------------------------------------------------------------
// void
// Agent::flowcolor(const char* color)
//----------------------------------------------------------------------
void
Agent::flowcolor(const char* color) {
 if (color[0] == 0) {
   if (flowcolor_) {
     delete []flowcolor_;
     flowcolor_ = 0;
   }
   return;
 }
       
 if (flowcolor_)
   delete []flowcolor_;

 flowcolor_ = new char[strlen(color) + 1];
 strcpy(flowcolor_, color);
}

void Agent::tracevar(const char* var)
{
        if (var[0] == 0) {
	       if (tracevar_) {
	              delete []tracevar_;
		      tracevar_ = 0;
	       }
	       return;
	}

        if (tracevar_)    delete []tracevar_;
	tracevar_ = new char[strlen(var) + 1];
	strcpy(tracevar_, var);
}

void Agent::reset(double)
{
	paint_ = Paint::instance()->thick();
}


void Agent::place(double x, double y) { 
	x_ = x;
	y_ = y;
	mark_ = 1;
	update_bb();
}

const char* Agent::info() const {
  static char text[128];

  if(AgentPartner_!=NULL) {
    sprintf(text, "Agent %s-%d\nConnected to: %s on Node %d",
                   name(), number(),
                   AgentPartner_->name(),
                   AgentPartner_->node_->num());
  } else {
    sprintf(text, "Agent: %s", label_);
  }
  return (text);
}

const char* Agent::getname() const
{
  static char text[128];
  sprintf(text, "a %s", label_);
  return (text);
}

void Agent::monitor(Monitor *m, double now, char *result, int len)
{
  char buf[256];
  Feature *f;
  monitor_=m;
  sprintf(result, "Agent: %s", label_);
  for(f=features_;f!=NULL;f=f->next()) {
    strcat(result, "\n");
    f->monitor(now, buf, 255);
    if (strlen(result)+strlen(buf)<(unsigned int)len)
      strcat(result, buf);
    else {
      fprintf(stderr, "not enough space for monitor return string\n");
      return;
    }
  }
}

//----------------------------------------------------------------------
// BoxAgent::BoxAgent(const char * type, int id, double size);
//----------------------------------------------------------------------
BoxAgent::BoxAgent(const char * type, int id, double size) :
  Agent(type,id,size) {
  BoxAgent::size(size);
}

//----------------------------------------------------------------------
// BoxAgent::BoxAgent(const char* name, double size)
//----------------------------------------------------------------------
BoxAgent::BoxAgent(const char* name, double size) :
  Agent(name, size) {
  BoxAgent::size(size);
}

//----------------------------------------------------------------------
// void
// BoxAgent::size(double s)
//----------------------------------------------------------------------
void
BoxAgent::size(double s) {
//  double delta = 0.5 * s;
  
  // Set size_ to s
  Agent::size(s);

 // x0_ = x_ - delta;
 // y0_ = y_ - delta;
 // x1_ = x_ + delta;
 // y1_ = y_ + delta;
 
  x0_ = x_;
  y0_ = y_;
//  x1_ = x_ + s;
//  y1_ = y_ + s*0.5;
  width_ = s;
  height_ = s*0.5;

 update_bb();
}

void BoxAgent::update_bb() {
	bb_.xmin = x0_;
	bb_.ymin = y0_;
	bb_.xmax = x0_ + width_;
	bb_.ymax = y0_ + height_;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void 
BoxAgent::findClosestCornertoPoint(double x, double y, double &corner_x,
                                   double &corner_y) const {
  double distance_0, distance_1;
  // Calculate the difference
  distance_0 = x0_ - x;
  distance_1 = x0_ + width_ - x;

  //We only want the magnitude
  if (distance_0 < 0) {
    distance_0 = -distance_0;
  }
  if (distance_1 < 0) {
    distance_1 = -distance_1;
  }
  if (distance_0 < distance_1) {
    corner_x = x0_;
  } else {
    corner_x = x0_ + width_;
  }

  // Do the same for the y values
  distance_0 = y0_ - y;
  distance_1 = y0_ + height_ - y;
  if (distance_0 < 0) {
    distance_0 = -distance_0;
  }
  if (distance_1 < 0) {
    distance_1 = -distance_1;
  }
  if (distance_0 < distance_1) {
    corner_y = y0_;
  } else {
    corner_y = y0_ + height_;
  }
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
int 
BoxAgent::inside(double, float px, float py) const {
	return (px >= x0_ && px <= (x0_ + width_) &&
		      py >= y0_ && py <= (y0_ + height_));
}

//----------------------------------------------------------------------
// void 
// BoxAgent::draw(View* nv, double ) const
//   - Draw the agent label with a rectangle around it
//   - Also draw monitors associated with it
//----------------------------------------------------------------------
void 
BoxAgent::draw(View* nv, double time) {
  double corner_x, corner_y, partner_corner_x, partner_corner_y;
  double label_width, label_height, label_x, label_y;
  char full_label[128];

  // Draw line connecting this to its partner
  if (AgentPartner_) {
    if (draw_link_) {
      AgentPartner_->findClosestCornertoPoint(x(), y(), 
                                        partner_corner_x, partner_corner_y);

      findClosestCornertoPoint(partner_corner_x, partner_corner_y,
                               corner_x, corner_y);
      nv->line(corner_x, corner_y, 
               partner_corner_x, partner_corner_y,
               paint_);
    }
  }
    
        
  // Draw the label if it exists
  if (label_ != 0) {
    // Exapnd label to show partner id if connected
    if (AgentPartner_) {
      if (AgentRole_ == SOURCE) {
        sprintf(full_label, "%s - %d", label_, number_);
      } else {
        sprintf(full_label, "%s - %d", label_, AgentPartner_->number());
      }

    } else {
      sprintf(full_label, "%s", label_);
    }

    // We have to keep calculting the label width and height because
    // we have no way of knowing if the user has zoomed in or out 
    // on the current network view.
    label_height = 0.9 * height_;
    label_width = nv->getStringWidth(full_label, label_height);

    // Add 10% of padding to width for the box
    setWidth(1.1 * label_width);

    place(x_,y_);
    
    // Center label in box
    label_x = x0_ + (width_ - label_width);
    label_y = y0_ + (height_ - label_height);

    nv->string(full_label, label_x , label_y, label_height, NULL);
    
    update_bb();
  }
  
  // Draw the rectangle 
  nv->rect(x0_, y0_, x0_ + width_, y0_ + height_, paint_);

  if (monitor_ != NULL) {
    monitor_->draw(nv, (x0_ + x0_ + width_)/2, y0_ - height_);
  }
}


/*
void BoxAgent::draw(PSView* nv, double ) const
{
  nv->rect(x0_, y1_, x1_, y0_, paint_);
  if (label_ != 0)
    {
      nv->string((x0_+x1_)/2, (y0_+y1_)/2, size_*0.75, label_, ANCHOR_CENTER);
    }
}
*/

//----------------------------------------------------------------------
// void 
// BoxAgent::place(double x, double y) {
//----------------------------------------------------------------------
void 
BoxAgent::place(double x, double y) {
  double nsin, ncos;
  Agent::place(x, y);  // Save original x,y values
  SINCOSPI(angle_, &nsin, &ncos);

  /*XXX should really use the X-display width*/
  //width_ = strlen(label_)/2;
  //width_ = strlen(label_) * 2.0;
  
  // on solaris, sin(M_PI) != 0 !!!
  
  // angle_ equals 0 or M_PI
  if ((nsin < 1.0e-8) && (nsin > -1.0e-9)) {
    y0_ = y_ - 0.5 * height_;
    //y_ = y0_;
  
  // angle is in the first 2 quadrants
  // i.e. between 0 and M_PI
  } else if (nsin > 0) {
    y0_ = y_;
   
  // angle_ is greater than M_PI but less than 2M_PI
  } else {
    // Move down twice as far to make room for traffic source labels
    y0_ = y_ - 2*height_;
    //y_ = y0_;
  }

  // angle equals M_PI/2 or 3M_PI/2 
  if ((ncos < 1.0e-8) && (ncos > -1.0e-9)) {
    x0_ = x_ - width_;
    //x_ = x0_;

  // angle is in 1st or 4th quadrant
  } else if (ncos > 0) {
    x0_ = x_;
  
  //  angle is in 2nd or 3rd quadrant
  //  i.e. between M_PI/2 and 3M_PI/2
  } else {
    x0_ = x_ - width_;
    //x_ = x0_;
  }

  update_bb();
}


//----------------------------------------------------------------------
// int
// Agent::writeNsDefinitionScript(FILE *file)
//----------------------------------------------------------------------
int
Agent::writeNsDefinitionScript(FILE *file) {
  TrafficSource * ts;

  // Create agent definition
  fprintf(file, "set agent(%d) [new Agent/%s]\n", number_, label_);
  // Attach it to it's parent node
  fprintf(file, "$ns attach-agent $node(%d) $agent(%d)\n",
                node_->num(), number_);

  if (AgentRole_ == SOURCE) {
    // Set flow color for agent
    fprintf(file, "\n$ns color %d \"%s\"\n", number_, flowcolor_);
    // flow id
    fprintf(file, "$agent(%d) set fid_ %d\n", number_, number_);
    // Packet Size
    fprintf(file, "$agent(%d) set packetSize_ %d\n", number_, packetSize_);

    if (strcmp(name(), "TCP") == 0) { 
      // Maximum window size
      fprintf(file, "$agent(%d) set window_ %d\n", number_, window_);
      // Initial reset of cwnd
      fprintf(file, "$agent(%d) set windowInit_ %d\n", number_, windowInit_);
      fprintf(file, "$agent(%d) set maxcwnd_ %d\n", number_, maxcwnd_);
      if (tracevar_) {
        if ((strcmp(tracevar_, "cwnd_") == 0) || 
            (strcmp(tracevar_, "ssthresh_") == 0)) {
          fprintf(file, "$ns add-agent-trace $agent(%d) tcp\n", number_);
          fprintf(file, "$ns monitor-agent-trace $agent(%d)\n", number_);
          fprintf(file, "$agent(%d) set tracevar_ %s\n", number_, tracevar_);
        }
      }
    } else if (strcmp(name(), "TCP/Reno") == 0) { 
    } else if (strcmp(name(), "TCP/NewReno") == 0) { 
    } else if (strcmp(name(), "TCP/Vegas") == 0) { 
    } else if (strcmp(name(), "TCP/Sack1") == 0) { 
    } else if (strcmp(name(), "TCP/Fack") == 0) { 
    } else if (strcmp(name(), "UDP") == 0) { 

    }
  } else if (AgentRole_ == DESTINATION) {
    // We have a sink
    if (strcmp(name(), "TCPSink") == 0) { 
      fprintf(file, "$agent(%d) set packetSize_ %d\n", number_, packetSize_);
    } else if (strcmp(name(), "TCPSink/DelAck") == 0) { 
      fprintf(file, "$agent(%d) set packetSize_ %d\n", number_, packetSize_);
      fprintf(file, "$agent(%d) set interval_ %f\n", number_, interval_);
    } else if (strcmp(name(), "TCPSink/Sack1") == 0) {
     // Ask about this one look at ns doc 29.2.3 for proper 
     // syntax
    }
    // if it is NULL or anything else do nothing
  }

  
   /*
  // CBR source agents
    } else if (strcmp(name(), "CBR")==0) {
      fprintf(file, "\n$ns color %d \"%s\"\n",number_, flowcolor_);
      fprintf(file, "set agent%d [new Agent/%s]\n", number_, label_);
      fprintf(file, "$ns attach-agent $n(%d) $agent%d \n", node_->num(), number_);
      fprintf(file, "$agent%d set fid_ %d\n", number_, number_);
      fprintf(file, "$agent%d set packetSize_ %d\n", number_, packetSize_);
      fprintf(file, "$agent%d set interval_ %f\n", number_, interval_);

  } else if (AgentRole_ == DESTINATION) {
    // TCP/Full destination agents 
    } else if (strcmp(name(), "TCP/FullTcp") == 0){
      fprintf(file, "\nset agent%d [new Agent/%s]\n", number_, label_);
      fprintf(file, "$ns attach-agent $node(%d) $agent%d \n", node_->num(), number_);
      fprintf(file, "$agent%d listen\n", number_);

    // all destination agents (NULL)
    } else {
      fprintf(file, "\nset agent%d [new Agent/%s]\n", number_, label_);
      fprintf(file, "$ns attach-agent $node(%d) $agent%d\n", node_->num(), number_);
    }
  }
*/
  if (traffic_sources_) {
      // # Create a CBR traffic source and attach it to udp0
      // set cbr0 [new Application/Traffic/CBR]
      // $cbr0 set packetSize_ 500
      // $cbr0 set interval_ 0.005
      // $cbr0 attach-agent $udp0
    fprintf(file, "\n# Create traffic sources and add them to the agent.\n");
    for (ts = traffic_sources_; ts; ts = ts->next_) {
      ts->writeNsDefinitionScript(file);
    }
  }

  return 0;
}

//----------------------------------------------------------------------
// int
// Agent::writeNsConnectionScript(FILE * file)
//----------------------------------------------------------------------
int
Agent::writeNsConnectionScript(FILE * file) {
  TrafficSource * ts;

  if (AgentRole_ == SOURCE) {
    fprintf(file, "$ns connect $agent(%d) $agent(%d)\n\n", 
                  number_, AgentPartner_->number_);

    fprintf(file, "\n# Traffic Source actions.\n");
    for (ts = traffic_sources_; ts; ts = ts->next_) {
      ts->writeNsActionScript(file);
    }
  }
  return 0;
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
int Agent::saveAsEnam(FILE *file) {
  if ((AgentRole_ == 10)&&(strcmp(name(), "CBR")!=0)) {
    if (AgentPartner_ != NULL) 
      fprintf(file, "##g -t * -l %s -r %d -s %d -c %s -i %d -w %d -m %d -t %s -b %f -p %d -o %f -n %d -u %d\n",
          label_, AgentRole_, node_->num(), flowcolor_, windowInit_, window_, 
          maxcwnd_, tracevar_, start_, produce_, stop_, number_, 
          AgentPartner_->number_);
    else 
      fprintf(file, "##g -t * -l %s -r %d -s %d -c %s -i %d -w %d -m %d -t %s -b %f -p %d -o %f -n %d\n",
          label_, AgentRole_, node_->num(), flowcolor_, windowInit_, window_,
          maxcwnd_, tracevar_, start_, produce_, stop_, number_);


  } else if (((AgentRole_ == SOURCE) || 
              (AgentRole_ == 10)) &&
             (strcmp(name(), "CBR")==0)) {
    if (AgentPartner_ != NULL)
      fprintf(file, "##g -t * -l %s -r %d -s %d -c %s -k %d -v %f -b %f -o %f -n %d -u %d \n",
           label_, AgentRole_, node_->num(), flowcolor_, packetSize_, 
           interval_, start_, stop_, number_, AgentPartner_->number_);
    else 
      fprintf(file, "##g -t * -l %s -r %d -s %d -c %s -k %d -v %f -b %f -o %f -n %d\n",
           label_, AgentRole_, node_->num(), flowcolor_, packetSize_,
           interval_, start_, stop_, number_);

  } else {
    if (AgentPartner_ != NULL)
      fprintf(file, "##g -t * -l %s -r %d -s %d -n %d -u %d \n",
                    label_, AgentRole_, node_->num(), number_,
                    AgentPartner_->number_);
    else 
      fprintf(file, "##g -t * -l %s -r %d -s %d -n %d\n",
                    label_, AgentRole_, node_->num(), number_);
  }
  return(0);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
const char* Agent::property() {
  rgb *color;
  color=Paint::instance()->paint_to_rgb(paint_);

  static char text[256];
  char * p;

  // Header
  p = text;
  sprintf(text, "{\"Agent: %s-%d\" title title \"Agent %d\"} ",
                name(), number_, number_);

  if (AgentRole_ == SOURCE) {
    p = &text[strlen(text)];
    sprintf(p, "{\"Flow Color\" flowcolor_ color %s} ", flowcolor_);
    p = &text[strlen(text)];
    sprintf(p, "{\"Packet Size\" packetSize_ text %d} ", packetSize_);
    
    //TCP source agents
    if (strcmp(name(), "TCP") == 0) {
      p = &text[strlen(text)];
      sprintf(p, "{\"Initial Window Size\" windowInit_ text %d} ", windowInit_);
      p = &text[strlen(text)];
      sprintf(p, "{\"Window\" window_ text %d} ", window_); 
      p = &text[strlen(text)];
      sprintf(p, "{\"Max Cwnd\" maxcwnd_ text %d} ", maxcwnd_);
    } 

  } else if (AgentRole_ == DESTINATION) {
    p = &text[strlen(text)];
    sprintf(p, "{\"Partner\" partner_number text %d} ", AgentPartner_->num());
  } else {
    p = &text[strlen(text)];
    sprintf(p, "{\"To edit this agent's properties connect agent to it's partner.\" partner_number label  }");
  }

  return(text);
}
