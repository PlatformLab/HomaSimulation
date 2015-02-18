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
 */

#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#endif

#include <ctype.h>
#include <math.h>

#include "bbox.h"
#include "netview.h"
#include "netmodel.h"
#include "tclcl.h"
#include "paint.h"
#include "packet.h"
#include "testview.h"

void
TestView::getWorldBox(BBox& world_boundary) {
  model_->BoundingBox(world_boundary);
}

void TestView::resize(int width, int height)
{
	width_ = width;
	height_ = height;

	matrix_.clear();

	BBox bb;

	bb.xmin=0;
	bb.ymin=0;
	bb.xmax=width;
	bb.ymax=height;

	BoundingBox(bb);

	double x = (0.0-panx_)*width;
	double y = (0.0-pany_)*height;
	double w = width;
	double h = height;
	double nw = bb.xmax - bb.xmin;
	double nh = bb.ymax - bb.ymin;
	double bbw;
	double bbh;

	if (aspect_==SQUARE) {
	  bbw = 1.1 * nw;
	  bbh = 1.1 * nh;
	} else {
	  bbw = nw;
	  bbh = nh;
	}

	matrix_.translate(-bb.xmin, -bb.ymin);
	
	double ws = w / bbw;
	double hs = h / bbh;

	if (aspect_==SQUARE) {
	  if (ws <= hs) {
	    matrix_.scale(ws, ws);
	    scale_=ws;
	    matrix_.translate(x, y + 0.5 * (h - ws * bbh));
	  } else {
	    matrix_.scale(hs, hs);
	    scale_=hs;
	    matrix_.translate(x + 0.5 * (w - hs * bbw), y);
	  }
	} else {
	  matrix_.scale(ws, hs);
	  matrix_.translate(x, y);
	}
  if ((width_<=0)||(height_<=0)) abort();
	matrix_.scale(magnification_, magnification_);
}

int count = 0;

void TestView::draw()
{
  int xmin, ymin, xmax, ymax;
  BBox bb;
  BoundingBox(bb);
  matrix_.map(bb.xmin, bb.ymin, xmin, ymin);
  matrix_.map(bb.xmax, bb.ymax, xmax, ymax);
  
  xmin+=36;
  ymin+=36;
  xmax+=36;
  ymax+=36;

  file_ = fopen(name_, "a");
	fprintf(file_, "\n--- snapshot (%d) ---\n", count);
  count++;
  
  model_->render(this);
  fclose(file_);
}

TestView::TestView(const char* name, NetModel* m)
    : View(name, SQUARE,200,200), model_(m), scale_(1.0)
{
  name_=new char[strlen(name)+1];
  strcpy(name_, name);
  resize(540,720);
  draw();	
}

void 
TestView::line(float x0, float y0, float x1, float y1, int paint)
{
	float ax, ay, bx, by;
	rgb * color;

	matrix_.map(x0, y0, ax, ay);
	matrix_.map(x1, y1, bx, by);
	color = Paint::instance()->paint_to_rgb(paint);
	fprintf(file_, "half line ");
	fprintf(file_, "from <%.1f %.1f> to <%.1f %.1f>\n",
          ax, ay, bx, by);
}

void TestView::rect(float x0, float y0, float x1, float y1, int paint)
{
	float x, y;
	matrix_.map(x0, y0, x, y);
	float xx, yy;
	matrix_.map(x1, y1, xx, yy);
	
	float w = xx - x;
	if (w < 0) {
		x = xx;
		w = -w;
	}
	float h = yy - y;
	if (h < 0) {
		h = -h;
		y = yy;
	}

  if (y>0) {
    fprintf(file_, "agent ");
    fprintf(file_, "at <x=%.1f y=%.1f> with <width=%.1f height=%.1f>\n", x, y, w, h);
  }
}

typedef struct floatpoint_s {
  float x, y;
} floatpoint;

void TestView::polygon(const float* x, const float* y, int n, int paint)
{
	floatpoint pts[10];
	int i;
  
	for (i = 0; i < n; ++i) {
		matrix_.map(x[i], y[i], pts[i].x, pts[i].y);
	}
	pts[n] = pts[0];
  
	fprintf(file_,"polygon node ");
	fprintf(file_,"at <%.1f %.1f>\n", pts[0].x, pts[0].y);
}

void TestView::fill(const float* x, const float* y, int n, int paint)
{
	floatpoint pts[10];
	int i;
  
	for (i = 0; i < n; ++i) {
		matrix_.map(x[i], y[i], pts[i].x, pts[i].y);
	}
	pts[n] = pts[0];
  
	fprintf(file_,"packet ");
	fprintf(file_, "at <%.1f %.1f>\n", pts[0].x, pts[0].y);
}

void TestView::circle(float x, float y, float r, int paint)
{
	float tx, ty;
	matrix_.map(x, y, tx, ty);
	float tr, dummy;
	matrix_.map(x + r, y, tr, dummy);
	tr -= tx;
  
	fprintf(file_, "circle node ");
	fprintf(file_, "at <%.1f %.1f> with <radius %.1f>\n", tx, ty, tr);
}

void TestView::string(float fx, float fy, float dim, const char* s, int anchor, 
                      const char*)
{
  float x, y;
  int d;

  matrix_.map(fx, fy, x, y);
  
  float dummy, dlow, dhigh;
  matrix_.map(0., 0., dummy, dlow);
  matrix_.map(0., 0.6 * dim, dummy, dhigh);
  d = int(dhigh - dlow);
  
  if(s!="") {
    switch (anchor) {
      case ANCHOR_CENTER:
        fprintf(file_, "with label (%s) at center <%.1f %.1f>\n", s, x, y);
        break;
      case ANCHOR_NORTH:
        fprintf(file_, "with label (%s) at north <%.1f %.1f>\n", s, x, y);
        break;
      case ANCHOR_SOUTH:
        fprintf(file_, "with label (%s) at south <%.1f %.1f>\n", s, x, y);
        break;
      case ANCHOR_WEST:
        fprintf(file_, "with label (%s) at west <%.1f %.1f>\n", s, x, y);
        break;
      case ANCHOR_EAST:
        fprintf(file_, "with label (%s) at east <%.1f %.1f>\n", s, x, y);
        break;
    }
  }
}





