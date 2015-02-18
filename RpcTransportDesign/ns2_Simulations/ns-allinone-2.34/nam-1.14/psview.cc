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
 * @(#) $Header: /cvsroot/nsnam/nam-1/psview.cc,v 1.10 2001/02/23 05:56:51 mehringe Exp $ (LBL)
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
#include "psview.h"

void PSView::zoom(float mag)
{
        magnification_ *= mag;
        resize(width_, height_);
}

void PSView::resize(int width, int height)
{
	width_ = width;
	height_ = height;

	matrix_.clear();

	BBox bb;

	/*a model can choose to use these values, or can set its own*/
	bb.xmin=0;
	bb.ymin=0;
	bb.xmax=width;
	bb.ymax=height;

	BoundingBox(bb);

	double x = (0.0-panx_)*width;
	double y = (0.0-pany_)*height;
	double w = width;
	double h = height;
	/*
	 * Set up a transform that maps bb -> canvas.  I.e,
	 * bb -> unit square -> allocation, but which retains
	 * the aspect ratio.  Also, add a margin.
	 */
	double nw = bb.xmax - bb.xmin;
	double nh = bb.ymax - bb.ymin;
	
	/* 
	 * Grow a margin if we asked for square aspect ratio.
	 */
	double bbw;
	double bbh;
	if (aspect_==SQUARE) {
	  bbw = 1.1 * nw;
	  bbh = 1.1 * nh;
	} else {
	  bbw = nw;
	  bbh = nh;
	}
#ifdef NOTDEF
	double tx = bb.xmin - 0.5 * (bbw - nw);
	double ty = bb.ymin - 0.5 * (bbh - nh);
#endif
	
	/*
	 * move base coordinate system to origin
	 */
#ifdef NOTDEF
	matrix_.translate(-tx, -ty);
#endif
	matrix_.translate(-bb.xmin, -bb.ymin);
	/*
	 * flip vertical axis because X is backwards.
	 */
	
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
	if (xscroll_!=NULL) {
	  Tcl& tcl = Tcl::instance();
	  tcl.evalf("%s set %f %f", xscroll_, panx_, 
		  panx_+(1.0/magnification_));
	}
	if (yscroll_!=NULL) {
	  Tcl& tcl = Tcl::instance();
	  tcl.evalf("%s set %f %f", yscroll_, pany_, 
		  pany_+(1.0/magnification_));
	}
}

void PSView::draw()
{
  int xmin, ymin, xmax, ymax;
  BBox bb;
  BoundingBox(bb);
  matrix_.map(bb.xmin, bb.ymin, xmin, ymin);
  matrix_.map(bb.xmax, bb.ymax, xmax, ymax);
  /*Ensure we have a half inch border*/
  xmin+=36;
  ymin+=36;
  xmax+=36;
  ymax+=36;

  file_ = fopen(name_, "w+");
  fprintf(file_, "%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(file_, "%%%%Creator: nam\n");
  fprintf(file_, "%%%%DocumentFonts: Helvetica\n");
  fprintf(file_, "%%%%BoundingBox: %d %d %d %d\n", xmin, ymin, xmax, ymax);
  fprintf(file_, "%%%%EndComments\n");
  fprintf(file_, "gsave 36 36 translate\n");
  fprintf(file_, "/namdict 20 dict def\n");
  fprintf(file_, "namdict begin\n");
  fprintf(file_, "/ft {/Helvetica findfont exch scalefont setfont\n");
  fprintf(file_, "newpath 0 0 moveto (0) true charpath flattenpath\n");
  fprintf(file_, "pathbbox /fh exch def pop pop pop} def\n");
  fprintf(file_, "/center_show {/s exch def moveto s stringwidth\n");
  fprintf(file_, "pop 2 div neg fh 2 div neg rmoveto s show} def\n");
  fprintf(file_, "/north_show {/s exch def moveto s stringwidth\n");
  fprintf(file_, "pop 2 div neg fh neg rmoveto s show} def\n");
  fprintf(file_, "/south_show {/s exch def moveto s stringwidth\n");
  fprintf(file_, "pop 2 div neg 0 rmoveto s show} def\n");
  fprintf(file_, "/east_show {/s exch def moveto s stringwidth\n");
  fprintf(file_, "pop neg fh 2 div neg rmoveto s show} def\n");
  fprintf(file_, "/west_show {/s exch def moveto \n");
  fprintf(file_, "0 fh 2 div neg rmoveto s show} def\n");
  fprintf(file_, "/rect {newpath /h exch def /w exch def moveto\n");
  fprintf(file_, "w 0 rlineto 0 h rlineto w neg 0 rlineto\n");
  fprintf(file_, "0 h neg rlineto closepath stroke} def\n");
  fprintf(file_, "/line {newpath moveto lineto stroke} def\n");
  fprintf(file_, "/circle {newpath 0 360 arc stroke} def\n");
  fprintf(file_, "end\n");
  fprintf(file_, "%%%%EndProlog\n");
  fprintf(file_, "namdict begin\n");
  model_->render(this);
  fprintf(file_, "showpage\n");
  fprintf(file_, "end\n");
  fprintf(file_, "%%%%Trailer\n");
  fclose(file_);
}

PSView::PSView(const char* name, NetModel* m)
  : View(name, SQUARE,200,200), model_(m), scale_(1.0)
{
  name_=new char[strlen(name)+1];
  strcpy(name_, name);
  resize(540,720);
  draw();
}


void PSView::getWorldBox(BBox & world_boundary) {
          model_->BoundingBox(world_boundary);
}


extern void Parse(NetModel*, const char* layout);

int PSView::command(ClientData cd, Tcl_Interp* tcl, int argc, char **argv)
{
  PSView *pv = (PSView *)cd;
  if (argc < 2) {
    Tcl_AppendResult(tcl, "\"", argv[0], "\": arg mismatch", 0);
    return (TCL_ERROR);
  }
  if (strcmp(argv[1], "xscroll") == 0) {
    if (argc == 3) {
      pv->xscroll_=new char[strlen(argv[2])+1];
      strcpy(pv->xscroll_, argv[2]);
      return TCL_OK;
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
                       "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  if (strcmp(argv[1], "yscroll") == 0) {
    if (argc == 3) {
      pv->yscroll_=new char[strlen(argv[2])+1];
      strcpy(pv->yscroll_, argv[2]);
      return TCL_OK;
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
                       "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  if (strcmp(argv[1], "zoom") == 0) {
    if (argc == 3) {
      float mag=atof(argv[2]);
      if (mag>1.0) {
	pv->panx_+=(1.0-1.0/mag)/(2.0*pv->magnification_);
	pv->pany_+=(1.0-1.0/mag)/(2.0*pv->magnification_);
      } else {
	pv->panx_-=(1.0-mag)/(2.0*pv->magnification_*mag);
	pv->pany_-=(1.0-mag)/(2.0*pv->magnification_*mag);
      }
      pv->zoom(mag);
      pv->draw();
      //pv->pan(panx, pany);
      return TCL_OK;
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
		       "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  if ((strcmp(argv[1], "xview") == 0)||(strcmp(argv[1], "yview")==0)) {
    if ((argc==4)&&(strcmp(argv[2], "moveto")==0)) {
      if (strcmp(argv[1], "xview") == 0)
	pv->panx_=atof(argv[3]);
      else
	pv->pany_=atof(argv[3]);
      pv->resize(pv->width_, pv->height_);
      pv->draw();
    } else if ((argc==5)&&(strcmp(argv[2], "scroll")==0)) {
      float step=atof(argv[3]);
      if (strcmp(argv[4], "units")==0) {
	step*=0.05/pv->magnification_;
      } else if (strcmp(argv[4], "pages")==0) {
	step*=0.8/pv->magnification_;
      }
      if (strcmp(argv[1], "xview") == 0)
	pv->panx_+=step;
      else
	pv->pany_+=step;
      pv->resize(pv->width_, pv->height_);
      pv->draw();
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
		       "\": arg mismatch", 0);
      return TCL_ERROR;
    }
    return TCL_OK;
  }
  Tcl_AppendResult(tcl, "\"", argv[0], "\": unknown arg: ", argv[1], 0);
  return (TCL_ERROR);
}


void 
PSView::line(float x0, float y0, float x1, float y1, int paint)
{
	float ax, ay;
	matrix_.map(x0, y0, ax, ay);
	float bx, by;
	matrix_.map(x1, y1, bx, by);
#ifdef NOTDEF
	GC gc = Paint::instance()->paint_to_gc(paint);
	XDrawLine(Tk_Display(tk_), offscreen_, gc, ax, ay, bx, by);
#endif
	rgb *color= Paint::instance()->paint_to_rgb(paint);
	fprintf(file_, "%.2f %.2f %.2f setrgbcolor\n", color->red/65536.0,
		color->green/65536.0,
		color->blue/65536.0);
	fprintf(file_, "%.1f %.1f %.1f %.1f line\n",
		ax, ay, bx, by);
}

void PSView::rect(float x0, float y0, float x1, float y1, int paint)
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
#ifdef NOTDEF
	GC gc = Paint::instance()->paint_to_gc(paint);
	XDrawRectangle(Tk_Display(tk_), offscreen_, gc, x, y, w, h);
#endif
  if (x>0 && y>0) {
    rgb *color= Paint::instance()->paint_to_rgb(paint);
    fprintf(file_, "%.2f %.2f %.2f setrgbcolor\n", color->red/65536.0,
            color->green/65536.0,
            color->blue/65536.0);
    fprintf(file_, "%.1f %.1f %.1f %.1f rect\n", x, y, w, h);
  }
}

typedef struct floatpoint_s {
  float x, y;
} floatpoint;

void PSView::polygon(const float* x, const float* y, int n, int paint)
{
	/*XXX*/
	floatpoint pts[10];
	int i;

	for (i = 0; i < n; ++i) {
		matrix_.map(x[i], y[i], pts[i].x, pts[i].y);
	}
	pts[n] = pts[0];
#ifdef NOTDEF	
	GC gc = Paint::instance()->paint_to_gc(paint);
	XDrawLines(Tk_Display(tk_), offscreen_, gc, pts, n + 1,
		   CoordModeOrigin);
#endif
	fprintf(file_,"%%polygon\n");
	rgb *color= Paint::instance()->paint_to_rgb(paint);
	fprintf(file_, "%.2f %.2f %.2f setrgbcolor\n", color->red/65536.0,
		color->green/65536.0,
		color->blue/65536.0);
	fprintf(file_, "newpath %.1f %.1f moveto\n", pts[0].x, pts[0].y);
	for(i=1; i<=n; i++)
	  fprintf(file_, "%.1f %.1f lineto\n", pts[i].x, pts[i].y);
	fprintf(file_, "closepath stroke\n");
}

void PSView::fill(const float* x, const float* y, int n, int paint)
{
	/*XXX*/
	floatpoint pts[10];
	int i;

	for (i = 0; i < n; ++i) {
		matrix_.map(x[i], y[i], pts[i].x, pts[i].y);
	}
	pts[n] = pts[0];
#ifdef NOTDEF
	GC gc = Paint::instance()->paint_to_gc(paint);
	XFillPolygon(Tk_Display(tk_), offscreen_, gc, pts, n + 1,
		     Convex, CoordModeOrigin);
#endif
	fprintf(file_,"%%fill\n");
	rgb *color= Paint::instance()->paint_to_rgb(paint);
	fprintf(file_, "%.2f %.2f %.2f setrgbcolor\n", color->red/65536.0,
		color->green/65536.0,
		color->blue/65536.0);
	fprintf(file_, "newpath %.1f %.1f moveto\n", pts[0].x, pts[0].y);
	for(i=1; i<=n; i++)
	  fprintf(file_, "%.1f %.1f lineto\n", pts[i].x, pts[i].y);
	fprintf(file_, "fill\n");
}

void PSView::circle(float x, float y, float r, int paint)
{
	float tx, ty;
	matrix_.map(x, y, tx, ty);
	float tr, dummy;
	matrix_.map(x + r, y, tr, dummy);
	tr -= tx;
#ifdef NOTDEF
	GC gc = Paint::instance()->paint_to_gc(paint);
	XDrawArc(Tk_Display(tk_), offscreen_, gc, tx, ty, tr, tr, 0, 64 * 360);
#endif
	rgb *color= Paint::instance()->paint_to_rgb(paint);
	fprintf(file_, "%.2f %.2f %.2f setrgbcolor\n", color->red/65536.0,
		color->green/65536.0,
		color->blue/65536.0);
	fprintf(file_, "%.1f %.1f %.1f circle\n", tx, ty, tr);
}

// We'll ignore string color in PS view!
void PSView::string(float fx, float fy, float dim, const char* s, int anchor, 
		    const char*)
{
  float x, y;
  matrix_.map(fx, fy, x, y);

  float dummy, dlow, dhigh;
  matrix_.map(0., 0., dummy, dlow);
  matrix_.map(0., 0.6 * dim, dummy, dhigh);
  int d = int(dhigh - dlow);
  fprintf(file_, "%d ft\n", d);

  fprintf(file_, "0 0 0 setrgbcolor\n");
  switch (anchor) {
  case ANCHOR_CENTER:
    fprintf(file_, "%.1f %.1f (%s) center_show\n", x, y, s);
    break;
  case ANCHOR_NORTH:
    fprintf(file_, "%.1f %.1f (%s) north_show\n", x, y, s);
    break;
  case ANCHOR_SOUTH:
    fprintf(file_, "%.1f %.1f (%s) south_show\n", x, y, s);
    break;
  case ANCHOR_WEST:
    fprintf(file_, "%.1f %.1f (%s) west_show\n", x, y, s);
    break;
  case ANCHOR_EAST:
    fprintf(file_, "%.1f %.1f (%s) east_show\n", x, y, s);
    break;
  }
}





