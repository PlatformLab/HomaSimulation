/*
 * Copyright (c) 2001 University of Southern California.
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

#include <stdlib.h>
#include <math.h>
#ifdef WIN32
#include <windows.h>
#endif
#include "netview.h"
#include "node.h"
#include "edge.h"
#include "paint.h"
#include "sincos.h"
#include "queuehandle.h"

#define PROPERTY_STRING_LENGTH 256

static int g_queuehandle_number = 1;

//----------------------------------------------------------------------
//  Wrapper for creating QueueHandles in OTcl
//----------------------------------------------------------------------
static class QueueHandleClass : public TclClass {
public:
  QueueHandleClass() : TclClass("QueueHandle") {}

  TclObject* create(int argc, const char * const * argv) {
    const char * type;
    int id;
    double size;

    type = argv[4];
    id = strtol(argv[5], NULL, 10);
    size = strtod(argv[6], NULL);
    
    return (new QueueHandle(type, id, size));
  }
} class_queuehandle;

//----------------------------------------------------------------------
//----------------------------------------------------------------------
QueueHandle::QueueHandle(const char* type, int id, double _size) :
  Animation(0, 0) {

  setDefaults();
  number_ = id;
  if (g_queuehandle_number++ <= id) {
    g_queuehandle_number = id + 1;
  }

  width_ = 50.0;
  height_ = size_/2.0;

  size_ = _size;
  size(_size);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
QueueHandle::QueueHandle(Edge * edge) :
  Animation(0, 0) {
  Node * node;

  setDefaults();
  edge_ = edge;
  
  number_ = g_queuehandle_number;
  g_queuehandle_number++;

  node = edge_->getSourceNode();
  if (node) {
   size(node->size());
  } else {
   size(10.0);
  }

  limit_ = 20;
  
  // FQ
  secsPerByte_ = 0.0;

  // Stocastic Fair Queue
  maxqueue_ = 40;
  buckets_ = 16;

  // DRR
  blimit_ = 2500;
  quantum_ = 250;
  mask_ = false;

  // RED
  bytes_ = false;
  queue_in_bytes_ = false;
  thresh_ = 5.0;
  maxthresh_ = 15.0;
  mean_pktsize_ = 500;
  q_weight_ = 0.002;
  wait_ = false;
  linterm_ = 30.0;
  setbit_ = false;
  drop_tail_ = false;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
QueueHandle::setDefaults() {
  next_queue_handle_ = NULL;
  edge_ = NULL;

  x_ = 0.0;
  y_ = 0.0;
  angle_ = NO_ANGLE;
  color_ = "black";
  paint_ = Paint::instance()->thin();
  width_ = 10.0;
  height_ = 10.0;

  type_ = NULL;
  ns_type_ = NULL;
  setType("DropTail");
}

//----------------------------------------------------------------------
// double 
// QueueHandle::distance(double x, double y) const {
//----------------------------------------------------------------------
double
QueueHandle::distance(double x, double y) const {
  return sqrt((x_-x)*(x_-x) + (y_-y)*(y_-y));
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
QueueHandle::color(const char * name) {
  if (color_) {
    delete []color_;
  }
  color_ = new char[strlen(name) + 1];
  strcpy(color_, name);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void QueueHandle::size(double s) {
  size_ = s;
  width_ = 10.0 + 25.0;
  height_ = s/2.0;
  update_bb();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void QueueHandle::update_bb() {
  bb_.xmin = x_;
  bb_.ymin = y_;
  bb_.xmax = x_ + width_;
  bb_.ymax = y_ + height_;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void QueueHandle::setType(const char* type) {
        const char * expanded_type;

        // Expand Ns Type Names 
	if (!strcmp(type, "FQ")) {
          expanded_type = "FairQueue";
	} else if (!strcmp(type, "SFQ")) {
          expanded_type = "StocasticFairQueue";
	} else if (!strcmp(type, "DRR")) {
          expanded_type = "DeficitRoundRobin";
        } else {
          expanded_type = type;
        }

  
        // Deleted existing type strings
	if (type_) {
		delete[] type_;
	}
	if (ns_type_) {
		delete[] ns_type_;
		ns_type_ = NULL;
	}

        // Set type and ns type to new strings
	type_ = new char[strlen(expanded_type) + 1];
	strcpy(type_, expanded_type);

	if (!strcmp(expanded_type, "DropTail")) {
		ns_type_ = new char[strlen("DropTail") + 1];
		strcpy(ns_type_, "DropTail");
	} else if (!strcmp(expanded_type, "FairQueue")) {
		ns_type_ = new char[strlen("FQ") + 1];
		strcpy(ns_type_, "FQ");
	} else if (!strcmp(expanded_type, "StocasticFairQueue")) {
		ns_type_ = new char[strlen("SFQ") + 1];
		strcpy(ns_type_, "SFQ");
	} else if (!strcmp(expanded_type, "DeficitRoundRobin")) {
		ns_type_ = new char[strlen("DRR") + 1];
		strcpy(ns_type_, "DRR");
	} else if (!strcmp(expanded_type, "RED")) {
		ns_type_ = new char[strlen("RED") + 1];
		strcpy(ns_type_, "RED");
	}
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void QueueHandle::reset(double) {
  paint_ = Paint::instance()->thick();
}

//----------------------------------------------------------------------
// void
// QueueHandle::attach(Agent * agent)
//----------------------------------------------------------------------
void
QueueHandle::attachTo(Edge * edge) {
  edge_ = edge; 
  place();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void QueueHandle::clearEdge() {
  edge_ = NULL;
}

//----------------------------------------------------------------------
// void
// QueueHandle::place()  
//----------------------------------------------------------------------
void
QueueHandle::place() {
	double sine, cosine, angle;

	if (edge_) {
		angle = edge_->angle();
		sine = sin(angle);
		cosine = cos(angle);

		if ((angle > 0.0 && angle < M_PI/2.0) ||
		    (angle < M_PI/-2.0 && angle > -1.0*M_PI)) {
			x_ = edge_->x() + cosine * height_;
			y_ = edge_->y() + sine * height_ - height_;
		} else {
			x_ = edge_->x() + cosine * height_;
			y_ = edge_->y() + sine * height_;
		}

	}
     update_bb();
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
const char* QueueHandle::info() const {
  static char text[128];

  sprintf(text, "Queue Type: %s %d->%d", type_, edge_->src(), edge_->dst());
  return (text);
}


//----------------------------------------------------------------------
// int 
// QueueHandle::inside(double, float px, float py) const 
//   - Check to see if point (px, py) is within the Loss Model box
//----------------------------------------------------------------------
int 
QueueHandle::inside(double, float px, float py) const {
  return (px >= x_ && 
          px <= (x_ + width_) &&
          py >= y_ && 
          py <= (y_ + height_));
}

//----------------------------------------------------------------------
// void
// QueueHandle::draw(View * view, double ) const
//   - Draw the Loss Model on its edge
//----------------------------------------------------------------------
void
QueueHandle::draw(View * view, double time) {
	double label_width, label_height, label_x, label_y;
	char * label = "Q";

	if (edge_) {

		// Draw label centered inside of border
		// We have to keep calculting the label width and height because
		// we have no way of knowing if the user has zoomed in or out 
		// on the current network view.
		label_height = 0.9 * height_;
		label_width = view->getStringWidth(label, label_height);

		// Add 10% of padding to width for the box
		setWidth(1.1 * label_width);

		// Center label in box
		label_x = x_ + (width_ - label_width);
		label_y = y_ + (height_ - label_height);

		view->string(label, label_x , label_y, label_height, NULL);
	
		update_bb();

		// Draw Rectangle Border
		view->rect(x_, y_, x_ + width_, y_ + height_ , paint_);
	}
}

//----------------------------------------------------------------------
// int
// QueueHandle::writeNsScript(FILE * file)
//  - outputs ns script format for creating loss models
//----------------------------------------------------------------------
int
QueueHandle::writeNsScript(FILE * file) {
	if (edge_) {
		fprintf(file, "# Set Queue Properties for link %d->%d\n", edge_->src(), edge_->dst());
		if (!strcmp(type_, "DropTail")) {
			fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set limit_ %d\n", edge_->src(), edge_->dst(), limit_);

		} else if (!strcmp(type_, "FairQueue")) {
			fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set secsPerByte_ %f\n", edge_->src(), edge_->dst(), secsPerByte_);
		} else if (!strcmp(type_, "StocasticFairQueue")) {
			fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set maxqueue_ %d\n", edge_->src(), edge_->dst(), maxqueue_);
			fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set buckets_ %d\n", edge_->src(), edge_->dst(), buckets_);
		} else if (!strcmp(type_, "DeficitRoundRobin")) {
			fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set buckets_ %d\n", edge_->src(), edge_->dst(), buckets_);
			fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set blimit_ %d\n", edge_->src(), edge_->dst(), blimit_);
			fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set quantum_ %d\n", edge_->src(), edge_->dst(), quantum_);
                        if (mask_) {
			        fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set mask_ 1\n", edge_->src(), edge_->dst());
                        }
		} else if (!strcmp(type_, "RED")) {
                        if (bytes_) {
			        fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set bytes_ 1\n", edge_->src(), edge_->dst());
                        }
                        if (queue_in_bytes_) {
			        fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set queue_in_bytes_ 1\n", edge_->src(), edge_->dst());
                        }
			fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set thresh_ %f\n", edge_->src(), edge_->dst(), thresh_);
			fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set maxthresh_ %f\n", edge_->src(), edge_->dst(), maxthresh_);
			fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set mean_pktsize_ %d\n", edge_->src(), edge_->dst(), mean_pktsize_);
			fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set q_weight_ %f\n", edge_->src(), edge_->dst(), q_weight_);
                        if (wait_) {
			        fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set queue_in_bytes_ 1\n", edge_->src(), edge_->dst());
                        }
			fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set linterm_ %f\n", edge_->src(), edge_->dst(), linterm_);
                        if (setbit_) {
			        fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set setbit_ 1\n", edge_->src(), edge_->dst());
                        }
                        if (queue_in_bytes_) {
			        fprintf(file, "[[$ns link $node(%d) $node(%d)] queue] set drop_tail_ 1\n", edge_->src(), edge_->dst());
                        }
		}
	}

  return 0;
}

//----------------------------------------------------------------------
// QueueHandle::property()
//   - return the list of queue configuration parameters
//     (properties) to be shown in the property edit window
//----------------------------------------------------------------------
const char*
QueueHandle::property() {
  return getProperties(type_);
}
  
//----------------------------------------------------------------------
// QueueHandle::property()
//   - return the list of queue configuration parameters
//     (properties) to be shown in the property edit window
//   - This is used if the queue type is changed to a different type
//     so we can give the properties related to that specific type of 
//     queue to which we have switched 
//----------------------------------------------------------------------
const char *
QueueHandle::getProperties(char * type) {
  static char text[PROPERTY_STRING_LENGTH];
  char * property_list;
  int total_written = 0;

  // object type, id and header info
  property_list = text;
  total_written = sprintf(text,
          "{\"Queue: %s %d->%d\" title title \"QueueHandle %d-%d\"} ",
          type_, edge_->src(), edge_->dst(), edge_->src(), edge_->dst());

  property_list = &text[strlen(text)];
  sprintf(property_list, "{\"Queue Type\" type_ drop_down_list {{%s} {DropTail FairQueue StocasticFairQueue DeficitRoundRobin RED}}} ", type);

  if (strcmp(type, "DropTail") == 0) {
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Maximum Queue Size\" limit_ text %d} ",
                           limit_);

  } else if (strcmp(type, "FairQueue") == 0) {
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Seconds Per Byte\" secsPerByte_ text %f} ",
                           secsPerByte_); 

  } else if (strcmp(type, "StocasticFairQueue") == 0) {
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Maximum Queue Size\" maxqueue_ text %d} ",
                           maxqueue_);
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Number of Buckets\" buckets_ text %d} ",
                           buckets_);

  } else if (strcmp(type, "DeficitRoundRobin") == 0) {
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Number of Buckets\" buckets_ text %d} ",
                           buckets_);
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Shared Buffer Size\" blimit_ text %d} ",
                           blimit_);
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Flow Quantum\" quantum_ text %d} ",
                           quantum_);
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Exclude Port Number from Flow Identification\" mask_ checkbox %d} ",
                           mask_);


  } else if (strcmp(type, "RED") == 0) {
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Enable Byte Mode RED\" bytes_ checkbox %d} ",
                           bytes_);
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Queue in Bytes\" queue_in_bytes_ checkbox %d} ",
                           queue_in_bytes_);
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Wait Interval Between Drops\" wait_ checkbox %d} ",
                           wait_);
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Mark Packets for Congestion Instead of Dropping\" setbit_ checkbox %d} ",
                           setbit_);
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Use Drop Tail instead of Random Drop\" drop_tail_ checkbox %d} ",
                           drop_tail_);

    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Minimum Threshhold for Average Queue Size\" thresh_ text %g} ", 
                           thresh_); 
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Maximum Threshhold for Average Queue Size\" maxthresh_ text %g} ", 
                           maxthresh_);
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Estimate for Average Queue Size\" mean_pktsize_ text %d} ", 
                           mean_pktsize_);
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Queue Weight\" q_weight_ text %g} ", 
                           q_weight_);
    property_list = &text[strlen(text)];
    sprintf(property_list, "{\"Maximum Drop Probability (1/linterm_)\" linterm_ text %g} ", 
                           linterm_);
  }

  return(text);
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
int QueueHandle::command(int argc, const char * const * argv) {
  return TCL_OK;
}
