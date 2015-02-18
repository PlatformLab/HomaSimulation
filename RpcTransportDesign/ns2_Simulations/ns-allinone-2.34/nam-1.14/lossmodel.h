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

#ifndef nam_lossmodel_h
#define nam_lossmodel_h


#include <tclcl.h>
#include "animation.h"


class LossModel : public Animation, public TclObject {
public:
  LossModel(const char * type, int id, double _size);
  LossModel(const char * name, double _size);

  inline int number() {return number_;}
  virtual int classid() const { return ClassLossModelID; }
  inline const char * name() const {return (label_);}
  virtual void reset(double);

  void attachTo(Edge * edge);
  void clearEdge();

  inline double x() const {return x_;}
  inline double y() const {return y_;}
  inline double width() const {return width_;}
  inline double height() const {return height_;}
 
  void setWidth(double w) {width_ = w;}
  void setHeight(double h) {height_ = h;}


  void place();
  const char* info() const;

  void label(const char* name);

  virtual double distance(double x, double y) const;
  void color(const char* name);
  virtual void size(double s);
  inline double size() const {return (size_);}

  const char* getname() const;

  int inside(double, float, float) const;
  virtual void update_bb();

  virtual void draw(View * view, double now);

  int writeNsDefinitionScript(FILE *file);

  const char * property(); 
  int command(int argc, const char * const * argv);

  void setPeriod(double period) {period_ = period;}
  void setOffset(double offset) {offset_ = offset;}
  void setBurstLength(double burst_length) {burstlen_ = burst_length;}
  void setRate(double rate) {rate_ = rate;}
  void setLossUnit(const char * unit);

private:
  void setDefaults(); 

public:
  LossModel * next_lossmodel_;  // Used by editornetmodel to track
                                // all traffic sources for property
                                // editing purposes
  Edge * edge_;


protected:
  int number_;

  double width_;
  double height_;
  double x_, y_;
  double angle_;
  double size_;
  char * label_;
  char * color_;

  char * loss_unit_;

  // Periodic Properties
  double period_, offset_, burstlen_;

  // Uniform Properties
  double rate_;
  
};



#endif
