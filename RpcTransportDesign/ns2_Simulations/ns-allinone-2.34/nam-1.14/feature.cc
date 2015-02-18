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
 * @(#) $Header: /cvsroot/nsnam/nam-1/feature.cc,v 1.8 2001/04/18 00:14:15 mehringe Exp $ (LBL)
 */

#ifdef WIN32
#include <windows.h>
#endif
#include "netview.h"
#include "agent.h"
#include "node.h"
#include "edge.h"
#include "paint.h"
#include "feature.h"

Feature::Feature(Agent *a, const char* name) :
	Animation(0, 0),
	next_(0),
	x_(0.),
	y_(0.),
	agent_(a),
	anchor_(0),
	mark_(0)
{
	varname_ = new char[strlen(name) + 1];
	strcpy(varname_, name);
	paint_ = Paint::instance()->thick();
}

void Feature::size(double s)
{
	size_ = s;
	update_bb();
}

void Feature::update_bb()
{
	double off = 0.5 * size_;
	/*XXX*/
	bb_.xmin = x_ - off;
	bb_.ymin = y_ - off;
	bb_.xmax = x_ + off;
	bb_.ymax = y_ + off;
}

int Feature::inside(double, float px, float py) const
{
	return (px >= bb_.xmin && px <= bb_.xmax &&
		py >= bb_.ymin && py <= bb_.ymax);
}

const char* Feature::info() const
{
	static char text[128];
	sprintf(text, "%s", varname_);
	return (text);
}

void Feature::monitor(double /*now*/, char *result, int /*len*/)
{
  sprintf(result, "This should not happen");
}

void Feature::varname(const char* name, int anchor)
{
	delete varname_;
	varname_ = new char[strlen(name) + 1];
	strcpy(varname_, name);
	anchor_ = anchor;
}

void Feature::drawlabel(View* nv) const
{
	/*XXX node number */
	if (varname_ != 0)
		nv->string(x_, y_, size_, varname_, anchor_);
}

void Feature::reset(double)
{
	paint_ = Paint::instance()->thick();
}

void Feature::place(double x, double y)
{ 
	x_ = x;
	y_ = y;
	mark_ = 1;
	update_bb();
}

ListFeature::ListFeature(Agent *a, const char* name) : 
  Feature(a,name), value_(NULL)
{
	ListFeature::size(a->size());
}

void ListFeature::set_feature(char *value)
{
  if (value_!=NULL)
    delete value_;
  value_ = new char[strlen(value) + 1];
  strcpy(value_, value);
}

void ListFeature::size(double s)
{
	Feature::size(s);
	double delta = 0.5 * s;
	x0_ = x_ - delta;
	y0_ = y_ - delta;
	x1_ = x_ + delta;
	y1_ = y_ + delta;
}

void ListFeature::monitor(double /*now*/, char *result, int /*len*/)
{
  sprintf(result, "%s:%s", varname_, value_);
}

void ListFeature::draw(View* nv, double now) {
	nv->rect(x0_, y0_, x1_, y1_, paint_);
	drawlabel(nv);
}

void ListFeature::place(double x, double y)
{
	Feature::place(x, y);

	double delta = 0.5 * size_;
	x0_ = x_ - delta;
	y0_ = y_ - delta;
	x1_ = x_ + delta;
	y1_ = y_ + delta;
}

VariableFeature::VariableFeature(Agent *a, const char* name) : 
  Feature(a,name), value_(NULL)
{
	VariableFeature::size(a->size());
}

void VariableFeature::set_feature(char *value)
{
  if (value_!=NULL)
    delete value_;
  value_ = new char[strlen(value) + 1];
  strcpy(value_, value);
}

void VariableFeature::size(double s)
{
	Feature::size(s);
	double delta = 0.5 * s;
	x0_ = x_ - delta;
	y0_ = y_ - delta;
	x1_ = x_ + delta;
	y1_ = y_ + delta;
}

void VariableFeature::monitor(double /*now*/, char *result, int /*len*/)
{
  sprintf(result, "%s:%s", varname_, value_);
}

void VariableFeature::draw(View* nv, double now) {
	nv->rect(x0_, y0_, x1_, y1_, paint_);
	drawlabel(nv);
}

void VariableFeature::place(double x, double y)
{
	Feature::place(x, y);

	double delta = 0.5 * size_;
	x0_ = x_ - delta;
	y0_ = y_ - delta;
	x1_ = x_ + delta;
	y1_ = y_ + delta;
}

TimerFeature::TimerFeature(Agent *a, const char* name) : 
  Feature(a,name)
{
  TimerFeature::size(a->size());
}

void TimerFeature::set_feature(double timer, int direction, double time_set)
{
  timer_=timer;
  direction_=direction;
  time_set_=time_set;
}

void TimerFeature::size(double s)
{
	Feature::size(s);
	double delta = 0.5 * s;
	x0_ = x_ - delta;
	y0_ = y_ - delta;
	x1_ = x_ + delta;
	y1_ = y_ + delta;
}

void TimerFeature::monitor(double /*now*/, char *result, int /*len*/)
{
  sprintf(result, "%s:%f", varname_, timer_);
}

void TimerFeature::draw(View* nv, double now) {
	nv->rect(x0_, y0_, x1_, y1_, paint_);
	drawlabel(nv);
}

void TimerFeature::place(double x, double y)
{
	Feature::place(x, y);

	double delta = 0.5 * size_;
	x0_ = x_ - delta;
	y0_ = y_ - delta;
	x1_ = x_ + delta;
	y1_ = y_ + delta;
}
