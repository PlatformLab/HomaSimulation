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
 * @(#) $Header: /cvsroot/nsnam/nam-1/edge.cc,v 1.54 2003/10/11 22:56:49 xuanc Exp $ (LBL)
 */


#include <tclcl.h>

#include "config.h"
#include "sincos.h"

#include "edge.h"
#include "queuehandle.h"
#include "node.h"
#include "packet.h"
#include "view.h"
#include "netview.h"
#include "psview.h"
#include "transform.h"
#include "paint.h"
#include "monitor.h"
#include "agent.h"
#include "lossmodel.h"

//----------------------------------------------------------------------
//  Wrapper for tcl creation of Edges (links) 
//----------------------------------------------------------------------
static class EdgeClass : public TclClass {
public:
  EdgeClass() : TclClass("Edge") {} 

  TclObject * create(int argc, const char*const* argv) {
    // set <Edge> [new Node node_id type size]
    Edge * edge;
    double bandwidth, delay, length, angle, label_size;
    bandwidth = strtod(argv[4], NULL);
    delay  = strtod(argv[5], NULL);
    length = strtod(argv[6], NULL);
    angle = strtod(argv[7], NULL);
    label_size = strtod(argv[8], NULL);

    edge = new Edge(label_size, bandwidth, delay, length, angle);
    return edge;
  }
} class_edge;

//----------------------------------------------------------------------
// Edge::Edge(double ps, double bw, double delay,
//            double length, double angle)
//    - Creates an edge that is not attached to any nodes.
//      Used by nam editor when loading in an existing enam file.
//    - With this constructor, the nodes need to be added 
//      to this edge using Edge::attachNodes() 
//----------------------------------------------------------------------
Edge::Edge(double ps, double bw, double delay,
           double length, double angle) :
  Animation(0,0) {

  psize_ = ps;
  angle_ = angle;
  bandwidth_ = bw;
  delay_ = delay;
  length_ = length;
  state_ = UP;

  paint_ = Paint::instance()->thick();
  
  setupDefaults(); 
}



//----------------------------------------------------------------------
// Edge::Edge(Node* src, Node* dst, double ps, double bw,
//            double delay, double length, double angle)
//   - creates an edge which is a one way network link
//   - the edge is automaticall attached to nodes src and dst
//----------------------------------------------------------------------
Edge::Edge(Node * src, Node * dst, double ps, double bw,
           double _delay, double length, double angle) :
  Animation(0,0),
  psize_(ps),
  angle_(angle),
  bandwidth_(bw),
  delay_(_delay),
  length_(length) {

  setupDefaults();

  src_ = src->num();
  dst_ = dst->num();
  start_ = src;
  neighbor_ = dst;

  paint_ = Paint::instance()->thick();
  if (length_ == 0) {
    length_ = delay_;
  }

}

//----------------------------------------------------------------------
// Edge::Edge(Node* src, Node* dst, double ps, double bw,
//            double delay, double length, double angle, int ws)
//   - creates an edge for wireless 
//----------------------------------------------------------------------
Edge::Edge(Node* src, Node* dst, double ps,
     double bw, double _delay, double length, double angle, int ws) :
  Animation(0,0),
  psize_(ps),
  angle_(angle),
  bandwidth_(bw),
  delay_(_delay),
  length_(length),
  wireless_(ws) 
{
  setupDefaults();

  src_ = src->num();
  dst_ = dst->num();
  start_ = src;
  neighbor_ = dst;

  paint_ = Paint::instance()->thick();
  if (length_ == 0) {
    length_ = delay_;
  }
}

//----------------------------------------------------------------------
// void
// Edge::setupDefaults()
//----------------------------------------------------------------------
void
Edge::setupDefaults() {
	x0_ = 0.0;
	y0_ = 0.0;
	state_ = UP;
	packets_ = NULL;
	no_of_packets_ = 0;
	last_packet_ = NULL;
	marked_ = 0;
	visible_ = 1;
	dlabel_ = NULL;
	dcolor_ = NULL;
	direction_ = 0;
	used_ = 0;
	
	// Set initial (x0,y0), (x1,y1) so that they reflect the current 
	// angle. This enables netmodel to share the same placeEdge() and
	// placeAgent() with AutoNetModel.
	SINCOSPI(angle_, &x1_, &y1_);

	lossmodel_ = NULL;
	queue_handle_ = NULL;	
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
Edge::~Edge() {
  if (queue_handle_) {
    delete queue_handle_;
  }
}

//----------------------------------------------------------------------
// void
// Edge::attachNodes(Node * source, Node * destination)
//----------------------------------------------------------------------
void
Edge::attachNodes(Node * source, Node * destination) {
  src_ = source->num();
  dst_ = destination->num();
  start_ = source;
  neighbor_ = destination;

  if (length_ == 0) {
    length_ = delay_;
  }
  start_->add_link(this);
}


//----------------------------------------------------------------------
// void
// Edge::addLossModel(LossModel * lossmodel)
//----------------------------------------------------------------------
void
Edge::addLossModel(LossModel * lossmodel) {
	if (!lossmodel_) {
		lossmodel_ = lossmodel;
		lossmodel_->attachTo(this);
		lossmodel_->place();
	}
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
LossModel * 
Edge::getLossModel() {
	return lossmodel_;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
Edge::clearLossModel() {
	// Delete previous loss model if it exists
	if (lossmodel_) {
		lossmodel_ = NULL;
	}
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
Edge::addQueueHandle(QueueHandle * q_handle) {
	queue_handle_ = q_handle;
	queue_handle_->place();
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
float Edge::distance(float x, float y) const 
{
	// TODO: Look it up when back home...
	matrix_.imap(x, y);
	return y;
}

//----------------------------------------------------------------------
// inline double
// Edge::delay() const
//   - delay_ is stored in milliseconds
//   - this function returns the delay in seconds
//
//   - look at tcl/netModel.tcl layout_link and layout_lan
//     for the conversion to milliseconds
//
//   - delay_ is kept in milliseconds so that Edge::info()
//     is displayed in millseconds and so that delay can be
//     entered in milliseconds by the nam editor
//
//----------------------------------------------------------------------
double
Edge::delay() const {
	return (delay_/1000.0);
} 

void Edge::init_color(const char *clr)
{
	color(clr);
        dcolor(clr);
	oldPaint_ = paint_;
}

void Edge::set_down(char *color)
{
	// If current color is down, don't change it again.
	// Assuming only one down color. User can change this behavior
	// by adding tcl code for link-up and link-down events.
	if (state_ == UP) {
		int pno = Paint::instance()->lookup(color, 3);
		oldPaint_ = paint_;
		paint_ = pno;
		state_ = DOWN;
	}
}

void Edge::set_up()
{
	if (state_ == DOWN) {
		state_ = UP;
		toggle_color();
	}
}

void Edge::place(double x0, double y0, double x1, double y1)
{
	x0_ = x0;
	y0_ = y0;
	x1_ = x1;
	y1_ = y1;

	double dx = x1 - x0;
	double dy = y1 - y0;
	/*XXX*/
	// delay should not be equal to edge's position
	//	delay_ = sqrt(dx * dx + dy * dy);
	matrix_.clear();
	matrix_.rotate((180 / M_PI) * atan2(dy, dx));
	matrix_.translate(x0, y0);

 	if (x1>x0) {
 		bb_.xmin = x0; bb_.xmax = x1;
	} else {
 		bb_.xmin = x1; bb_.xmax = x0;
 	}
 	if (y1>y0) {
 		bb_.ymin = y0; bb_.ymax = y1;
 	} else {
 		bb_.ymin = y1; bb_.ymax = y0;
 	}
	eb_.xmin = -0.1 * start_->size();
	eb_.xmax = sqrt(dx*dx+dy*dy) + 0.1*neighbor_->size(); 
	eb_.ymin = -0.1 * start_->size();
 	eb_.ymax = 0.1 * start_->size();

	if (queue_handle_) {
	  queue_handle_->place();
	}
	if (lossmodel_) {
		lossmodel_->place();
	}
}

void Edge::AddPacket(Packet *p)
{
	p->next(packets_);
	if (packets_!=NULL)
		packets_->prev(p);
	p->prev(NULL);
	packets_=p;
	if (last_packet_==NULL)
		last_packet_=p;
	no_of_packets_++;
#define PARANOID
#ifdef PARANOID
	int ctr=0;
	for(Packet *tp=packets_;tp!=NULL;tp=tp->next()) {
		ctr++;
		if (ctr>no_of_packets_) 
			abort();
	}
	if (last_packet_->next()!=NULL) 
		abort();
#ifdef DEBUG
	printf("AddPacket: %d->%d OK\n", src_, dst_);
#endif
#endif
#undef PARANOID
}

void Edge::arrive_packet(Packet *p, double atime)
{
	/*Trigger any arrival event at the node*/
	neighbor_->arrive_packet(p, this, atime);
}

void Edge::DeletePacket(Packet *p)
{
	/*Trigger any deletion event at the node*/
	if (neighbor_)  neighbor_->delete_packet(p);
	else return ;

	no_of_packets_--;
	if (last_packet_==p) {
		/*Normal case - it's the last packet*/
		last_packet_ = p->prev();
		if (packets_!=p)
			p->prev()->next(NULL);
		else
			packets_=NULL;
	} else {
		/*Abnormal case - need to search the list*/
		Packet *tp;
		tp=packets_;
		while((tp!=p)&&(tp!=NULL)) {
			tp=tp->next();
		}
		if (tp==p) {
			if (tp==packets_)
				/*it was the first packet*/
				packets_=tp->next();
			else
				tp->prev()->next(tp->next());
			if (tp==last_packet_) {
				/*this shouldn't ever happen*/
				fprintf(stderr, "**it happened\n");
				last_packet_=tp->prev();
			} else
				tp->next()->prev(tp->prev());
		}
	}
#define PARANOID
#ifdef PARANOID
	int ctr=0;
	for(Packet *tp=packets_;tp!=NULL;tp=tp->next()) {
		ctr++;
		if (ctr>no_of_packets_) abort();
	}
	if ((last_packet_!=NULL)&&(last_packet_->next()!=NULL)) abort();
#ifdef DEBUG
	printf("DeletePacket: %d->%d OK\n", src_, dst_);
#endif
#endif
#undef PARANOID
}

void Edge::dlabel(const char* name)
{
        if (name[0] == 0) {
                if (dlabel_) {
                        delete []dlabel_;
                        dlabel_ = 0;
                }
                return;
        }

        if (dlabel_)
                delete []dlabel_;
        dlabel_ = new char[strlen(name) + 1];
        strcpy(dlabel_, name);
}

void Edge::dcolor(const char* name)
{
  if (name[0] == 0) {
    if (dcolor_) {
                        delete []dcolor_;
                        dcolor_ = 0;
    }
                return;
  }

        if (dcolor_)
                delete []dcolor_;
        dcolor_ = new char[strlen(name) + 1];
        strcpy(dcolor_, name);
}

void Edge::direction(const char* name)
{
	if (name[0] == 0) {
		if (direction_)  
			direction_ = 0;
		return;
	}
        if (!strcmp(name, "SOUTH"))          direction_ = 1;
        else if (!strcmp(name, "NORTH"))     direction_ = 2;
        else if (!strcmp(name, "EAST"))      direction_ = 3;
        else if (!strcmp(name, "WEST"))      direction_ = 4;
        else                                 direction_ = 2;
}

void Edge::draw(View* view, double now) {
  if (visible()) {
	   view->line(x0_, y0_, x1_, y1_, paint_);
	}
	if (monitor_!=NULL)
	  monitor_->draw(view, (x0_+x1_)/2, (y0_+y1_)/2);

       	if (dlabel_ == 0) 
		return;
	// Draw dynamic label
	switch (direction_) {
	case 0:
		view->string((x0_+x1_)/2.0, (y0_+y1_)/2.0+psize_*1.5, 
			     psize_*2.3, dlabel_, direction_, dcolor_);
		break;
	case 1:
		view->string((x0_+x1_)/2.0, (y0_+y1_)/2.0-psize_*1.5, 
			     psize_*2.3, dlabel_, direction_, dcolor_);
		break;
	case 2:
		view->string((x0_+x1_)/2.0, (y0_+y1_)/2.0+psize_*1.5, 
			     psize_*2.3, dlabel_, direction_, dcolor_);
		break;
	case 3:
		view->string((x0_+x1_)/2.0-psize_*1.5, (y0_+y1_)/2.0, 
			     psize_*2.3, dlabel_, direction_, dcolor_);
		break;
	case 4:
		view->string((x0_+x1_)/2.0+psize_*1.5, (y0_+y1_)/2.0, 
			     psize_*2.3, dlabel_, direction_, dcolor_);
		break;
	default:
		view->string((x0_+x1_)/2.0, (y0_+y1_)/2.0+psize_*1.5, 
			     psize_*2.3, dlabel_, direction_, dcolor_);
		break;
	}
}

void Edge::reset(double)
{
	//	paint_ = Paint::instance()->thick();
	no_of_packets_=0;
	packets_=NULL;
	last_packet_=NULL;
}

int Edge::inside(double, float px, float py) const
{
//	matrix_.imap(px, py);
//	return eb_.inside(px, py);
	return inside(px, py);
}

int Edge::inside(float px, float py) const
{
	matrix_.imap(px, py);
	return eb_.inside(px, py);
}

const char* Edge::info() const
{
	static char text[128];
	sprintf(text, "Link: %d<->%d\n  Bandwidth: %g Mbits/sec\n  Delay: %g ms\n",
		src_, dst_, bandwidth_, delay_);
	return (text);
}

const char* Edge::property() {
  rgb *color;
  color=Paint::instance()->paint_to_rgb(paint_);
 
  static char text[256];
  char *p;
 
  // object type and id
  p = text;
  if (!wireless_) {
    sprintf(text, "{\"Link %d-%d\" title title \"Link %d-%d\"} ", 
	    src_, dst_, src_, dst_);
  } else {
    sprintf(text, "{\"Wireless Link %d-%d\" title title \"Wireless Link %d-%d\"} ", 
	    src_, dst_, src_, dst_);
  }

  p = &text[strlen(text)];
  sprintf(p, "{Color color_ color %s} ", color->colorname); // color & value

  // label
  p = &text[strlen(text)];
  if (dlabel_) {
    sprintf(p, "{Label dlabel_ text %s} ", dlabel_); 
  } else {
    sprintf(p, "{Label dlabel_ text  } "); 
  }
  
  //p = &text[strlen(text)];
  //sprintf(p, "{LABEL-DIRECTION %d} ", direction_); // label-direction $ value
  
  // Link Bandwidth
  p = &text[strlen(text)];
  sprintf(p, "{\"Bandwidth (Mbits per second)\" bandwidth_ text %-f} ", bandwidth_);

  // Link Delay
  p = &text[strlen(text)];
  sprintf(p, "{\"Delay (ms)\" delay_ text %-f} ", delay_);

  return(text);
}

const char* Edge::getname() const
{
	static char text[128];
	sprintf(text, "l %d %d",
		src_, dst_);
	return (text);
}

void Edge::monitor(Monitor *m, double /*now*/, char *result, int /*len*/)
{
	monitor_=m;
	sprintf(result, "link %d-%d:\n bw: %g Mbits/sec\n delay: %g ms",
		src_, dst_, bandwidth_, delay_);
	return;
}

int Edge::save(FILE *file)
{
	rgb *color;
	color=Paint::instance()->paint_to_rgb(paint_);
	char state[10];
	double angle,length;
	float dx,dy;
	//Edges in the tracefile are bidirectional, so don't need to save
	//both unidirectional edges
	if (src_>dst_) return 0;

	if (state_==UP)
		strcpy(state, " -S UP");
	else if (state_==DOWN)
		strcpy(state, " -S DOWN");
	else
		state[0]='\0';
	if (angle_<0)
		angle=180*(angle_+2*M_PI)/M_PI;
	else
		angle=180*(angle_/M_PI);
	dx=x0_-x1_;
	dy=y0_-y1_;
	length=sqrt(dx*dx+dy*dy);
	//  printf("%d->%d, angle=%f (%fdeg)\n", src_, dst_, angle_, angle);
	fprintf(file, "l -t * -s %d -d %d -r %f -D %f -c %s -o %.1fdeg -l %f%s\n", 
		src_, dst_, bandwidth_, delay_, color->colorname,angle, length, state);
	return 0;
}

int Edge::saveAsEnam(FILE *file)
{
	rgb *color;
	color=Paint::instance()->paint_to_rgb(paint_);
	char state[10];
	double angle,length;
	float dx,dy;
	// XXX Should never set edge size outside scale_estimate()!!!!
	//    psize_ = 25.0;

	// Edges in the tracefile are bidirectional, so don't need to save
	// both unidirectional edges
	if (src_>dst_) return 0; 
 
	if (state_==UP)
		strcpy(state, " -S UP");
	else if (state_==DOWN)
		strcpy(state, " -S DOWN");
	else
		state[0]='\0';
	if (angle_<0)
		angle=180*(angle_+2*M_PI)/M_PI;
	else
		angle=180*(angle_/M_PI);
	dx=x0_-x1_;
	dy=y0_-y1_;
	length=sqrt(dx*dx+dy*dy);
	//  printf("%d->%d, angle=%f (%fdeg)\n", src_, dst_, angle_, angle);
	fprintf(file, "##l -t * -s %d -d %d -r %f -D %f -c %s -o %.1fdeg -l %f%s -b %s\n",
		src_, dst_, bandwidth_, delay_, color->colorname,angle, length, state, dlabel_);
	return 0;
}

//----------------------------------------------------------------------
// int
// Edge::writeNsScript(FILE *file)
//   - Edges in the tracefile are bidirectional, so don't need to save
//     both unidirectional edges
//----------------------------------------------------------------------
int
Edge::writeNsScript(FILE *file) {
  rgb *color;
  double angle,length;
  float dx,dy;

  color = Paint::instance()->paint_to_rgb(paint_); 

  if (angle_ < 0) { 
     angle = 180 * (angle_ + 2 * M_PI) / M_PI;
   } else {
     angle = 180 * (angle_ / M_PI);
   }
   dx = x0_ - x1_;
   dy = y0_ - y1_;
   length = sqrt(dx * dx + dy * dy);

   // Create the link
   if (queue_handle_) {
     //fprintf(file, "$ns simplex-link $node(%d) $node(%d) %f %f DropTail\n",
     fprintf(file, "$ns simplex-link $node(%d) $node(%d) %fMb %fms %s\n",
                    src_, dst_, bandwidth_, delay_, queue_handle_->name());
   } else {
     fprintf(file, "$ns simplex-link $node(%d) $node(%d) %fMb %fms DropTail\n",
                    src_, dst_, bandwidth_, delay_);
   }

   // Add a packet queue
   fprintf(file, "$ns simplex-link-op $node(%d) $node(%d) queuePos 0.5\n",
                  src_, dst_);

   // Set different options for the link
   // - Color
   fprintf(file, "$ns simplex-link-op $node(%d) $node(%d) color %s\n",
                  src_, dst_, color->colorname);
   // - Orientation
   fprintf(file, "$ns simplex-link-op $node(%d) $node(%d) orient %.1fdeg\n",
                  src_, dst_, angle);
   
   if (dlabel_){
     // - label
     fprintf(file, "$ns simplex-link-op $node(%d) $node(%d) label %s\n",
                    src_, dst_, dlabel_);
   }

   if (direction_!=0) {
     // - label orientation
     fprintf(file, "$ns simplex-link-op $node(%d) $node(%d) label-at %d\n",
                    src_, dst_, direction_);
   }

   if (queue_handle_) {
     queue_handle_->writeNsScript(file);
   }

   fprintf(file, "\n");

  return 0;
}

