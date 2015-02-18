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
#include "edge.h"
#include "paint.h"
#include "sincos.h"
#include "lossmodel.h"
#include "queuehandle.h"

#define PROPERTY_STRING_LENGTH 256

static int g_lossmodel_number = 1;

//----------------------------------------------------------------------
//  Wrapper for creating LossModels in OTcl
//----------------------------------------------------------------------
static class LossModelClass : public TclClass {
public:
  LossModelClass() : TclClass("LossModel") {}

  TclObject* create(int argc, const char * const * argv) {
    const char * type;
    int id;
    double size;

    type = argv[4];
    id = strtol(argv[5], NULL, 10);
    size = strtod(argv[6], NULL);
    
    return (new LossModel(type, id, size));
  }
} class_lossmodel;

//----------------------------------------------------------------------
//----------------------------------------------------------------------
LossModel::LossModel(const char* type, int id, double _size) :
  Animation(0, 0) {

  setDefaults();
  number_ = id;
  if (g_lossmodel_number++ <= id) {
    g_lossmodel_number = id + 1;
  }

  label_ = new char[strlen(type) + 1];
  strcpy(label_, type);

  width_ = 50.0 * strlen(type);
  height_ = size_/2.0;

  size_ = _size;
  size(_size);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
LossModel::LossModel(const char* name, double _size) :
  Animation(0, 0) {

  setDefaults();
  
  number_ = g_lossmodel_number;
  g_lossmodel_number++;

  width_ = 50.0 * strlen(name);
  height_ = size_/2.0;
  size_ = _size;
  label_ = new char[strlen(name) + 1];
  strcpy(label_, name);
  size(_size);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
LossModel::setDefaults() {
  next_lossmodel_ = NULL;
  edge_ = NULL;

  x_ = 0.0;
  y_ = 0.0;
  angle_ = NO_ANGLE;
  color_ = "black";
  paint_ = Paint::instance()->thin();

  loss_unit_ = NULL;
  setLossUnit("pkt");

  // Periodic Parameters
  period_ = 1.0;
  offset_ = 0.0;
  burstlen_ = 0.0;

  // Uniform Parameters
  rate_ = 0.1;
}

//----------------------------------------------------------------------
// double 
// LossModel::distance(double x, double y) const {
//----------------------------------------------------------------------
double
LossModel::distance(double x, double y) const {
  return sqrt((x_-x)*(x_-x) + (y_-y)*(y_-y));
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
LossModel::color(const char * name) {
  if (color_) {
    delete []color_;
  }
  color_ = new char[strlen(name) + 1];
  strcpy(color_, name);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void LossModel::size(double s) {
  size_ = s;
  width_ = 10.0 + 25.0*strlen(label_);
  height_ = s/2.0;
  update_bb();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void LossModel::update_bb() {
  bb_.xmin = x_;
  bb_.ymin = y_;
  bb_.xmax = x_ + width_;
  bb_.ymax = y_ + height_;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void LossModel::label(const char* name) {
  if (label_) {
    delete label_;
  }
  label_ = new char[strlen(name) + 1];
  strcpy(label_, name);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void LossModel::reset(double) {
  paint_ = Paint::instance()->thick();
}

//----------------------------------------------------------------------
// void
// LossModel::attach(Agent * agent)
//   - Attach this loss model to an edge
//----------------------------------------------------------------------
void
LossModel::attachTo(Edge * edge) {
  edge_ = edge; 
  place();
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void LossModel::clearEdge() {
  edge_->clearLossModel();
  edge_ = NULL;
}

//----------------------------------------------------------------------
// void
// LossModel::place()  
//   - place the loss model on the edge to which it is attached.
//----------------------------------------------------------------------
void
LossModel::place() {
  QueueHandle * queue_handle;
  if (edge_) {
    // Place loss model on top of the queue handle
    queue_handle = edge_->getQueueHandle();
    
    // Place loss model on top of queue handle
    x_ = queue_handle->x();
    y_ = queue_handle->y() + queue_handle->height(); 

    // Place loss model to the right of queue handle
    //x_ = queue_handle->x() + queue_handle->width();
    //y_ = queue_handle->y(); 
  }
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
const char* LossModel::info() const {
  static char text[128];

  sprintf(text, "Loss Model: %s %d->%d", label_, edge_->src(), edge_->dst());
  return (text);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
const char* LossModel::getname() const
{
  static char text[128];
  sprintf(text, "a %s", label_);
  return (text);
}


//----------------------------------------------------------------------
// int 
// LossModel::inside(double, float px, float py) const 
//   - Check to see if point (px, py) is within the Loss Model box
//----------------------------------------------------------------------
int 
LossModel::inside(double, float px, float py) const {
  return (px >= x_ && 
          px <= (x_ + width_) &&
          py >= y_ && 
          py <= (y_ + height_));
}

//----------------------------------------------------------------------
// void
// LossModel::draw(View * view, double ) const
//   - Draw the Loss Model on its edge
//----------------------------------------------------------------------
void
LossModel::draw(View * view, double time) {
	double label_width, label_height, label_x, label_y;
	static char full_label[128];

	// Draw label centered inside of border
	if (label_) {
		//sprintf(full_label, "%s: %d->%d", label_, edge_->src(), edge_->dst());
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
	view->rect(x_, y_, x_ + width_, y_ + height_ , paint_);
}

//----------------------------------------------------------------------
// int
// LossModel::writeNsDefinitionScript(FILE * file)
//  - outputs ns script format for creating loss models
//----------------------------------------------------------------------
int
LossModel::writeNsDefinitionScript(FILE * file) {
  if (!strcmp(name(), "Periodic")) {
    fprintf(file, "set loss_model(%d) [new ErrorModel/%s]\n",
                   number_, label_);
    // Connect loss model to edge
    fprintf(file, "$ns lossmodel $loss_model(%d) $node(%d) $node(%d)\n", number_,
            edge_->src(), edge_->dst());

    // Set Properties
    fprintf(file, "$loss_model(%d) unit %s\n", number_, loss_unit_);
    fprintf(file, "$loss_model(%d) set period_ %f\n", number_, period_);
    fprintf(file, "$loss_model(%d) set offset_ %f\n", number_, offset_);
    fprintf(file, "$loss_model(%d) set burstlen_ %f\n", number_, burstlen_);
    // Set Drop Target
    fprintf(file, "$loss_model(%d) drop-target [new Agent/Null]\n", number_);

  } else if (!strcmp(name(), "Expo")) {

  } else if (!strcmp(name(), "Uniform")) {
    fprintf(file, "set loss_model(%d) [new ErrorModel/%s %f %s]\n",
                   number_, label_, rate_, loss_unit_);
    
    // Connect loss model to edge
    fprintf(file, "$ns lossmodel $loss_model(%d) $node(%d) $node(%d)\n", number_,
            edge_->src(), edge_->dst());

    // Set Drop Target
    fprintf(file, "$loss_model(%d) drop-target [new Agent/Null]\n", number_);

  } else {
    fprintf(stderr, "Unknown loss model %s needs to be implemented.\n", label_);
  }
                          
  return 0;
}

//----------------------------------------------------------------------
// LossModel::property()
//   - return the list of loss model configuration parameters
//     (properties) to be shown in the property edit window
//----------------------------------------------------------------------
const char*
LossModel::property() {
  static char text[PROPERTY_STRING_LENGTH];
  char * property_list;
  int total_written = 0;

  property_list = text;
  total_written = sprintf(text, 
          "{\"Loss Model: %s %d->%d\" title title \"LossModel %d\"} ",
          name(), edge_->src(), edge_->dst(), number_);

  if (strcmp(name(), "Periodic") == 0) {
      property_list = &text[strlen(text)];
      sprintf(property_list, "{\"Period\" period_ text %f} ", period_);
      property_list = &text[strlen(text)];
      sprintf(property_list, "{\"Offset\" offset_ text %f} ", offset_); 
      property_list = &text[strlen(text)];
      sprintf(property_list, "{\"Burst Length\" burstlen_ text %f} ", burstlen_);

  } else if (strcmp(name(), "Expo") == 0) {

  } else if (strcmp(name(), "Uniform") == 0) {
      property_list = &text[strlen(text)];
      sprintf(property_list, "{\"Loss Rate\" rate_ text %f} ", rate_);
  }

  return(text);
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
int LossModel::command(int argc, const char * const * argv) {
  int length = strlen(argv[1]);

  if (strncmp(argv[1], "setRate", length) == 0) {
    setRate(strtod(argv[2],NULL));

  } else if (strncmp(argv[1], "setLossUnit", length) == 0) {
    setLossUnit(argv[2]);
  }

  return TCL_OK;
}

//----------------------------------------------------------------------
// void
// LossModel::setLossUnit(const char * unit) 
//   - sets the loss model's loss unit type (byte, pkt, etc...)
//----------------------------------------------------------------------
void
LossModel::setLossUnit(const char * unit) {
  if (loss_unit_) {
    delete []loss_unit_;
  }

  loss_unit_ = new char[strlen(unit) + 1];
  strcpy(loss_unit_, unit);
}
