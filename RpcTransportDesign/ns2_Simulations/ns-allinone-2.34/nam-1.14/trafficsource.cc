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
#include "psview.h"
#include "node.h"
#include "feature.h"
#include "agent.h"
#include "edge.h"
#include "paint.h"
#include "monitor.h"
#include "sincos.h"
#include "trafficsource.h"

#define PROPERTY_STRING_LENGTH 256

static int g_trafficsource_number = 1;

//----------------------------------------------------------------------
//  Wrapper for creating Traffic Sources in OTcl
//----------------------------------------------------------------------
static class TrafficSourceClass : public TclClass {
public:
  TrafficSourceClass() : TclClass("TrafficSource") {}

  TclObject* create(int argc, const char * const * argv) {
    const char * type;
    int id;
    double size;

    type = argv[4];
    id = strtol(argv[5], NULL, 10);
    size = strtod(argv[6], NULL);
    
    return (new TrafficSource(type, id, size));
  }
} class_trafficsource;

//----------------------------------------------------------------------
// TrafficSource::TrafficSource(const char* type, double _size)
//----------------------------------------------------------------------
TrafficSource::TrafficSource(const char* type, int id, double _size) :
  Animation(0, 0) {

  setDefaults();
  number_ = id;
  if (g_trafficsource_number++ <= id) {
    g_trafficsource_number = id + 1;
  }

  label_ = new char[strlen(type) + 1];
  strcpy(label_, type);

  width_ = 50.0 * strlen(type);
  height_ = size_/2.0;

  size_ = _size;
  size(_size);
}

//----------------------------------------------------------------------
// TrafficSource::TrafficSource(const char* name, double size)
//----------------------------------------------------------------------
TrafficSource::TrafficSource(const char* name, double _size) :
  Animation(0, 0) {

  setDefaults();
  
  number_ = g_trafficsource_number;
  g_trafficsource_number++;

  width_ = 50.0 * strlen(name);
  height_ = size_/2.0;
  size_ = _size;
  label_ = new char[strlen(name) + 1];
  strcpy(label_, name);
  size(_size);
  timelist.add(0.0);
  timelist.add(60.0);

  // Bind tcl variables to c++ ones
//  bind("start_", &start_);
//  bind("stop_", &stop_);
//  bind("packet_size_", &packet_size_);
//  bind("interval_", &interval_);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
TrafficSource::setDefaults() {
  previous_ = NULL;
  next_ = NULL;
  editornetmodel_next_ = NULL;
  agent_ = NULL;

  packet_size_ = 500;
  interval_ = 0.00195;

  // Exponential & Pareto
  burst_time_ = 500;
  idle_time_ = 500;
  rate_ = 100;

  // Pareto
  shape_ = 1.5;
  
  x_ = 0.0;
  y_ = 0.0;
  angle_ = NO_ANGLE;
  window_ = 20;
  windowInit_ = 1;
  maxcwnd_ = 0;
  color_ = "green";
  anchor_ = 0;
  paint_ = Paint::instance()->thin();

  maxpkts_ = 256;
}

//----------------------------------------------------------------------
// void
// TrafficSource::attach(Agent * agent)
//   - Attach this traffic source to an agents list of traffic sources
//----------------------------------------------------------------------
void
TrafficSource::attachTo(Agent * agent) {
  agent_ = agent;
  previous_ = agent_->addTrafficSource(this);
  size(agent_->size()); 
  place();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
TrafficSource::removeFromAgent() {
  // previous_ should be NULL for the first traffic source
  // attached to the agent
  agent_->removeTrafficSource(this);
  next_ = NULL;
  previous_ = NULL;
  agent_ = NULL;
}

//----------------------------------------------------------------------
// double 
// TrafficSource::distance(double x, double y) const {
//----------------------------------------------------------------------
double
TrafficSource::distance(double x, double y) const {
  return sqrt((x_-x)*(x_-x) + (y_-y)*(y_-y));
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
TrafficSource::color(const char * name) {
  if (color_) {
    delete []color_;
  }
  color_ = new char[strlen(name) + 1];
  strcpy(color_, name);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void TrafficSource::size(double s) {
  size_ = s;
  width_ = 10.0 + 25.0*strlen(label_);
  height_ = s/2.0;
  update_bb();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void TrafficSource::update_bb() {
  bb_.xmin = x_;
  bb_.ymin = y_;
  bb_.xmax = x_ + width_;
  bb_.ymax = y_ + height_;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void TrafficSource::label(const char* name) {
  if (label_) {
    delete label_;
  }
  label_ = new char[strlen(name) + 1];
  strcpy(label_, name);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void TrafficSource::drawlabel(View* view) const {
  // Check out anchor
  if (label_) {
    view->string(x_, y_, size_, label_, anchor_);
  }
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//void TrafficSource::drawlabel(PSView* nv) const {
//  if (label_)
//    nv->string(x_, y_, size_, label_, anchor_);
//}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void TrafficSource::reset(double) {
  paint_ = Paint::instance()->thick();
}

//----------------------------------------------------------------------
// void
// TrafficSource::place()  
//   - place the traffic source in relation to the agent  to 
//     which it is attached.
//----------------------------------------------------------------------
void
TrafficSource::place() {
  if (previous_) {
    placeNextTo(previous_);
  } else {
    placeOnAgent();
  }
}

//----------------------------------------------------------------------
// void 
// TrafficSource::placeNextTo(TrafficSource * neighbor)
//   - Place this traffic source to the right of its neighbor
//----------------------------------------------------------------------
void 
TrafficSource::placeNextTo(TrafficSource * neighbor)  {
  if (neighbor) {
    x_ = neighbor->x() + neighbor->width(); 
    y_ = neighbor->y(); 
    update_bb();
  }
}
//----------------------------------------------------------------------
// void
// TrafficSource::placeOnAgent() 
//   - Place this traffic source on top of the Agent
//----------------------------------------------------------------------
void
TrafficSource::placeOnAgent() { 
  if (agent_) {
    x_ = agent_->x();
    y_ = agent_->y() + agent_->height(); // Agent scales it's height, 
                                     // It's messy but works
    update_bb();
  }
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
const char* TrafficSource::info() const {
  static char text[128];

  sprintf(text, "TrafficSource: %s \nAttached to Agent: %s-%d",
                 label_, agent_->name(), agent_->number());
  return (text);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
double 
TrafficSource::stopAt() {
	return timelist.lastStopTime();
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
const char* TrafficSource::getname() const
{
  static char text[128];
  sprintf(text, "a %s", label_);
  return (text);
}


//----------------------------------------------------------------------
// int 
// TrafficSource::inside(double, float px, float py) const 
//   - Check to see if point (px, py) is within the Traffic Source box
//----------------------------------------------------------------------
int 
TrafficSource::inside(double, float px, float py) const {
  return (px >= x_ && 
          px <= (x_ + width_) &&
          py >= y_ && 
          py <= (y_ + height_));
}

//----------------------------------------------------------------------
// void
// TrafficSource::draw(View * view, double ) const
//   - Draw the Traffic Source on top of its parent agent and
//     after the other traffic sources before it
//----------------------------------------------------------------------
void
TrafficSource::draw(View * view, double time) {
	double label_width, label_height, label_x, label_y;
	int paint_color_id; 
	static char full_label[128];

	// Draw label centered inside of border
	if (label_) {
		//sprintf(full_label, "%s - %d", label_, number_);
		sprintf(full_label, "%s", label_);
		
		// We have to keep calculting the label width and height because
		// we have no way of knowing if the user has zoomed in or out 
		// on the current network view.
		label_height = 0.9 * height_;
		label_width = view->getStringWidth(full_label, label_height);

		// Add 10% of padding to width for the box
		setWidth(1.1 * label_width);
		
		// Center label in box
		label_x = x_ + (width_ - label_width);
		label_y = y_ + (height_ - label_height);

		view->string(full_label, label_x , label_y, label_height, NULL);
		
		update_bb();
	}

	
	// Draw Rectangle Border

	if (timelist.isOn(time)) {
		paint_color_id = Paint::instance()->lookup("darkgreen", 3);
		if (paint_color_id >= 0) {
			view->rect(x_, y_, x_ + width_, y_ + height_, paint_color_id);
		}
	} else {
	view->rect(x_, y_, x_ + width_, y_ + height_ , paint_);
	}
}

//----------------------------------------------------------------------
// int
// TrafficSource::writeNsDefinitionScript(FILE * file)
//  - outputs ns script format for creating traffic sources
//  - This only outputs the intialization part for the trafficsource
//    and attaches it to the agent, more dynamic control of the 
//    agent is outputed by int TrafficSource::writeNsActionScript
//----------------------------------------------------------------------
int
TrafficSource::writeNsDefinitionScript(FILE * file) {
  // CBR traffic source has a slightly different syntax
  if (!strcmp(name(), "CBR")) {
    fprintf(file, "set traffic_source(%d) [new Application/Traffic/%s]\n",
                   number_, label_);
    fprintf(file, "$traffic_source(%d) attach-agent $agent(%d)\n", 
                   number_, agent_->number());
//    fprintf(file, "$traffic_source(%d) set packetSize_ %d\n",
//                   number_, packet_size_);
    fprintf(file, "$traffic_source(%d) set interval_ %f\n",
                   number_, interval_);

  } else if (!strcmp(name(), "Exponential")) {
    fprintf(file, "set traffic_source(%d) [new Application/Traffic/%s]\n",
                   number_, label_);
    fprintf(file, "$traffic_source(%d) attach-agent $agent(%d)\n", 
                   number_, agent_->number());
//    fprintf(file, "$traffic_source(%d) set packetSize_ %d\n",
//                   number_, packet_size_);
    fprintf(file, "$traffic_source(%d) set burst_time_ %dms\n",
                   number_, burst_time_);
    fprintf(file, "$traffic_source(%d) set idle_time_ %dms\n",
                   number_, idle_time_);
    fprintf(file, "$traffic_source(%d) set rate_ %dk\n",
                   number_, rate_);

  } else if (!strcmp(name(), "FTP")) {
    fprintf(file, "set traffic_source(%d) [new Application/%s]\n",
                   number_, label_);
    fprintf(file, "$traffic_source(%d) attach-agent $agent(%d)\n", 
                   number_, agent_->number());
    fprintf(file, "$traffic_source(%d) set maxpkts_ %d\n",
                   number_, maxpkts_);

  } else if (!strcmp(name(), "Pareto")) {
    fprintf(file, "set traffic_source(%d) [new Application/Traffic/%s]\n",
                   number_, label_);
    fprintf(file, "$traffic_source(%d) attach-agent $agent(%d)\n", 
                   number_, agent_->number());
//    fprintf(file, "$traffic_source(%d) set packetSize_ %d\n",
//                   number_, packet_size_);
    fprintf(file, "$traffic_source(%d) set burst_time_ %dms\n",
                   number_, burst_time_);
    fprintf(file, "$traffic_source(%d) set idle_time_ %dms\n",
                   number_, idle_time_);
    fprintf(file, "$traffic_source(%d) set rate_ %dk\n",
                   number_, rate_);
    fprintf(file, "$traffic_source(%d) set shape_ %f\n",
                   number_, shape_);

  } else if (!strcmp(name(), "Telnet")) {
    fprintf(file, "set traffic_source(%d) [new Application/%s]\n",
                   number_, label_);
    fprintf(file, "$traffic_source(%d) attach-agent $agent(%d)\n", 
                   number_, agent_->number());
    fprintf(file, "$traffic_source(%d) set interval_ %f\n",
                   number_, interval_);
  }
                          
  return 0;
}

//----------------------------------------------------------------------
// int
// TrafficSource::writeNsActionScript(FILE * file)
//----------------------------------------------------------------------
int
TrafficSource::writeNsActionScript(FILE *file) {
	TimeElement * time;
	bool start = true;

//	if (!strcmp(name(), "CBR")) {
		for (time = timelist.head(); time; time = time->next_) {
			if (start) {
				fprintf(file, "$ns at %f \"$traffic_source(%d) start\"\n",
								time->time_, number_);
				start = false;
			} else {
				fprintf(file, "$ns at %f \"$traffic_source(%d) stop\"\n\n",
								time->time_, number_);
				start = true;
			}
		}

//	} else if (!strcmp(name(), "FTP")) {
//		for (time = timelist.head(); time; time = time->next_) {
//			if (start) {
//				fprintf(file, "$ns at %f \"$traffic_source(%d) start\"\n",
//								time->time_, number_);
//				start = false;
//			} else {
//				fprintf(file, "$ns at %f \"$traffic_source(%d) stop\"\n\n",
//								time->time_, number_);
//				start = true;
//			}
//		}
//	}

  return 0;
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
int TrafficSource::saveAsEnam(FILE *file) {
  return 0;
}

//----------------------------------------------------------------------
// TrafficSource::property()
//   - return the list of traffic source configuration parameters
//     (properties) to be shown in the property edit window
//----------------------------------------------------------------------
const char*
TrafficSource::property() {
  static char text[PROPERTY_STRING_LENGTH];
  char timelist_string[200];
  char * property_list;
  int total_written = 0;

  property_list = text;
  total_written = sprintf(text,
          "{\"Traffic Source %s %d\" title title \"TrafficSource %d\"} ",
          name(), number_, number_);

  property_list = &text[strlen(text)];
  total_written += sprintf(property_list,
                "{\"Agent %s\" agent_name label  } ", agent_->name());

  // ---- CBR -> Constant Bit Rate
  if (strcmp(name(), "CBR") == 0) {
    timelist.getListString(timelist_string, 200);
    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                            "{\"Start/Stop Time\" timelist timelist {%s}} ", 
                            timelist_string);

    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                      "{\"Interval\" interval_ text %f} ", interval_);

  // ---- Exponential
  } else if (!strcmp(name(), "Exponential")) {  // strcmp returns 0 when equal
    timelist.getListString(timelist_string, 200);
    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                            "{\"Start/Stop Time\" timelist timelist {%s}} ", 
                            timelist_string);

//    --- Why Packet size for both traffic generators and agents?
//    property_list = &text[strlen(text)];
//    total_written += sprintf(property_list,
//                      "{\"Interval\" packet_size_ text %d} ", packet_size_);

    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                      "{\"Burst Time (ms)\" burst_time_ text %d} ", burst_time_);

    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                      "{\"Idle Time (ms)\" idle_time_ text %d} ", idle_time_);

    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                      "{\"Rate (k)\" rate_ text %d} ", rate_);


  // ---- FTP
  } else if (!strcmp(name(), "FTP")) {  // strcmp returns 0 when equal
    timelist.getListString(timelist_string, 200);
    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                            "{\"Start/Stop Time\" timelist timelist {%s}} ", 
                            timelist_string);

    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
       "{\"Maximum Number of Packets to Send\" maxpkts_ text %d} ", maxpkts_);
  
  
  // ---- Pareto
  } else if (!strcmp(name(), "Pareto")) {  // strcmp returns 0 when equal
    timelist.getListString(timelist_string, 200);
    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                            "{\"Start/Stop Time\" timelist timelist {%s}} ", 
                            timelist_string);

//    --- Why Packet size for both traffic generators and agents?
//    property_list = &text[strlen(text)];
//    total_written += sprintf(property_list,
//                      "{\"Interval\" packet_size_ text %d} ", packet_size_);

    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                      "{\"Burst Time (ms)\" burst_time_ text %d} ", burst_time_);

    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                      "{\"Idle Time (ms)\" idle_time_ text %d} ", idle_time_);

    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                      "{\"Rate (k)\" rate_ text %d} ", rate_);

    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                      "{\"Shape\" shape_ text %f} ", shape_);


  // ---- Telnet
  } else if (!strcmp(name(), "Telnet")) {  // strcmp returns 0 when equal
    timelist.getListString(timelist_string, 200);
    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
                            "{\"Start/Stop Time\" timelist timelist {%s}} ", 
                            timelist_string);

    property_list = &text[strlen(text)];
    total_written += sprintf(property_list,
       "{\"Exponetial Distribution Interval\" interval_ text %f} ", interval_);
  }

  return(text);
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
TimeList::TimeList(double time) {
	head_ = NULL;
	add(time);
}

//----------------------------------------------------------------------
// TimeList::~TimeList()
//   - deletes the remaining part of the list
//----------------------------------------------------------------------
TimeList::~TimeList() {
	clear();
}

//----------------------------------------------------------------------
// void
// TimeList::clear() {
//   - deletes all items on the list
//----------------------------------------------------------------------
void
TimeList::clear() {
	TimeElement * run;

	for (run = head_->next_; run; run = run->next_) {
		delete head_;
		head_ = run;
	}

	if (head_) {
		delete head_;
	}
	head_ = NULL;
}

//----------------------------------------------------------------------
// TimeList *
// TimeList::add(double time) 
//    - adds a new time value to the list which is 
//      sorted in ascending order
//----------------------------------------------------------------------
void
TimeList::add(double time) {
	TimeElement * new_time, * run, * previous;
	
	if (!head_) {
		// First element on list
		head_ = new TimeElement(time);

	} else {
		// Have to place down on list
		previous = NULL;
		run = head_;
		for (run = head_; run; run = run->next_) {
			if (time == run->time_) {
				// No duplicate times allowed
				break;
			} else if (time < run->time_) {
				if (previous) {
					// Place in middle of list
					new_time = new TimeElement(time);
					new_time->next_ = run;
					previous->next_ = new_time;
				} else {
					// Add to front of list
					new_time = new TimeElement(time);
					new_time->next_ = head_;
					head_ = new_time;
				}
				break;
			}
			previous = run;
		}

		if (!run) {
			// We ran to the end of the list and couldn't place
			// the new time so we have to place it at the end 
			previous->next_ = new TimeElement(time);
		}
	}
}

//----------------------------------------------------------------------
// TimeList * 
// TimeList::remove(double time)
//    - removes time from the list 
//----------------------------------------------------------------------
void
TimeList::remove(double time) {
	TimeElement * run, * previous;
	previous = NULL;

	for (run = head_; run; run = run->next_) {
		if (run->time_ == time) {
			if (previous) {
				previous->next_ = run->next_;
				delete run;

			} else {
				// Element is at the front of the list
				head_ = run->next_;
			}
			break;
		}
		previous = run;
	}
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
TimeList::setList(const char * list_string) {
	const char *run;
	char *next;
	double time;
	// Erase all items on the list 
	clear();

	// Run down the list string and add values to the list
	run = list_string;
	while (run) {
		time = strtod(run, &next);
		if (run != next) {
			add(time);
		} else {
			break;
		}
		run = next;
	}
}

//----------------------------------------------------------------------
// void 
// TimeList::getListString(char * buffer, int buffer_size)
//   - fills buffer with a space delimited list of all values on the
//     time list sorted in ascending order
//
//   - the returned list should be freed when it is not needed anymore
//----------------------------------------------------------------------
void
TimeList::getListString(char * buffer, int buffer_size) {
	int total_written, just_written;
	TimeElement * run;

	total_written = 1; // Need space for the /0 character
	for (run = head_; run; run = run->next_) {
		just_written = sprintf(buffer, 
					"%f ", run->time_);

		if (just_written == -1) {
			fprintf(stderr, "Ran out of buffer space when creating time list string\n");
			break;
		}
		//Advance buffer pointer past written characters
		buffer += just_written;
		total_written += just_written;
	}
}

//----------------------------------------------------------------------
// bool
// TimeList::isOn(double time)
//   - checks the timelist to see if time falls between a
//     start-stop period or a stop-start period
//   - if it is start-stop the object should be on so it will return
//     true.  Otherwise the object is off and will return false.
//----------------------------------------------------------------------
bool
TimeList::isOn(double time) {
	TimeElement * element;
	bool on = false;

	for (element = head(); element; element = element->next_) {
		if (time < element->time_) {
				break;
		}

		if (on) {
			on = false;
		} else {
			on = true;
		}
	}
	return on;
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
double
TimeList::lastStopTime() {
	TimeElement * element;
	double stop_time = 0.0;
	bool start = true;

	for (element = head(); element; element = element->next_) {
		if (start) {
			start = false;
		} else {
			stop_time = element->time_;
			start = true;
		}
	}
	return stop_time;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
TimeElement::TimeElement(double time) {
	next_ = NULL;
	time_ = time;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
TimeElement::~TimeElement() {
	next_ = NULL;
}
