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
 * @(#) $Header: /cvsroot/nsnam/nam-1/view.cc,v 1.30 2003/10/11 22:56:51 xuanc Exp $ (LBL)
 */

#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#endif

#include <ctype.h>
#include <math.h>

#include <tclcl.h>
#include "view.h"
#include "bbox.h"
#include "netview.h"
#include "netmodel.h"
#include "tclcl.h"
#include "paint.h"
#include "packet.h"

// Create list of font names and sizes
static char * fontName[NFONT] = {
  "-adobe-times-medium-r-normal-*-8-*-*-*-*-*-*-*",
  "-adobe-times-medium-r-normal-*-10-*-*-*-*-*-*-*",
  "-adobe-times-medium-r-normal-*-12-*-*-*-*-*-*-*",
  "-adobe-times-medium-r-normal-*-14-*-*-*-*-*-*-*",
  "-adobe-times-medium-r-normal-*-18-*-*-*-*-*-*-*",
  "-adobe-times-medium-r-normal-*-20-*-*-*-*-*-*-*",
  "-adobe-times-medium-r-normal-*-24-*-*-*-*-*-*-*",
  "-adobe-times-medium-r-normal-*-34-*-*-*-*-*-*-*"
};


// Convert a string to world coordinates
int View::getCoord(char *strx, char *stry, float &dx, float &dy) {
	Tcl& tcl = Tcl::instance();

	double tx, ty;
	if ((Tk_GetScreenMM(tcl.interp(), tk_, strx, &tx) != TCL_OK) ||
	    (Tk_GetScreenMM(tcl.interp(), tk_, stry, &ty) != TCL_OK)) {
		return TCL_ERROR;
	}
	
	tx *= pixelsPerMM_;
	ty *= pixelsPerMM_;
	dx = tx, dy = ty;
	matrix_.imap(dx, dy);
	return TCL_OK;
}

void View::zoom(float mag)
{
        magnification_ *= mag;
        resize(width_, height_);
}

//---------------------------------------------------------------------
// void
// View::resize(int width, int height)
//   - resize the display canvas and setup the mapping between 
//     world and screen coordinate systems
//---------------------------------------------------------------------
void View::resize(int width, int height) {
  width_ = width;
  height_ = height;

  matrix_.clear();

  BBox bb;

  // a model can choose to use these values, or can set its own
  // NetModel sets it to bb.clear(), while GraphView use this
  bb.xmin = 0;
  bb.ymin = 0;
  bb.xmax = width;
  bb.ymax = height;

  // WorldBox refers to the boundaries for the network model world 
  // Basically NetView and EditView used to return the bounding box
  // for NetModel not for themselves so now BoundingBox returns
  // values based on the canvas size and getWorldBox returns the 
  // NetModel values.
  getWorldBox(bb);
 // BoundingBox(bb);

  double x = (0.0 - panx_) * width;
  double y = (0.0 - pany_) * height;
  double w = width;
  double h = height;
  /*
   * Set up a transform that maps bb -> canvas.  I.e,
   * bb -> unit square -> allocation, but which retains
   * the aspect ratio.  Also, add a margin.
   */
  double world_width = bb.xmax - bb.xmin;
  double world_height = bb.ymax - bb.ymin;

  
  /* 
   * Grow a margin if we asked for square aspect ratio.
   */
  double bbw;
  double bbh;

  if (aspect_ == SQUARE) {
    bbw = 1.1 * world_width;
    bbh = 1.1 * world_height;

   // bbw = world_width;
   // bbh = world_width*height/width;

 //   if (bbh < world_height) {
      // Need to scale in the other direction
  //    bbw = world_height*width/height;
   //   bbh = world_height;
   // }
  } else {
    bbw = world_width;
    bbh = world_height;
  }

  // Calculate the width and height for translating x and y axis
  double tx = bb.xmin - 0.5 * (bbw - world_width);
  double ty = bb.ymin - 0.5 * (bbh - world_height);
  
  // move base coordinate system to origin
  matrix_.translate(-tx, -ty);
  
  // flip vertical axis because Y is backwards.
  matrix_.scale(1.0, -1.0);
  matrix_.translate(0., bbh);
  
  double ws = w / bbw;
  double hs = h / bbh;
  
  // The matrix_.translate(x expression, y expression) takes care of
  // scrolling when clicking on the scroll bars
  if (aspect_ == SQUARE) {
    if (ws <= hs) {
      matrix_.scale(ws, ws);
      matrix_.translate(x, y + 0.5 * (h - ws * bbh));
    } else {
      matrix_.scale(hs, hs);
      matrix_.translate(x + 0.5 * (w - hs * bbw), y);
    }
  } else {
    matrix_.scale(ws, hs);
    matrix_.translate(x, y);
  }

  if (offscreen_ != 0)
    Tk_FreePixmap(Tk_Display(tk_), offscreen_);

  if ((width_<=0) || (height_<=0))
    abort();

  // Create a new offscreen canvas for double buffered drawing
  offscreen_ = Tk_GetPixmap(Tk_Display(tk_), Tk_WindowId(tk_),
                            width_, height_, Tk_Depth(tk_));


  // Scale for any magnification
  matrix_.scale(magnification_, magnification_);

  if (xscroll_ != NULL) {
    Tcl& tcl = Tcl::instance();
    tcl.evalf("%s set %f %f", xscroll_, panx_, 
                              panx_+(1.0/magnification_));
  }

  if (yscroll_ != NULL) {
    Tcl& tcl = Tcl::instance();
    tcl.evalf("%s set %f %f", yscroll_, pany_, 
                              pany_+(1.0/magnification_));
  }

  pixelsPerMM_ = WidthOfScreen(Tk_Screen(tk_)) / 
                 WidthMMOfScreen(Tk_Screen(tk_));

  if (!bClip_) {
    clip_.xmin = 0;
    clip_.ymin = 0;
    clip_.xmax = width_;
    clip_.ymax = height_;
  }
}

//---------------------------------------------------------------------
// void
// View::draw() 
//   - Does double buffered drawing
//     All objects are drawn to an offscreen area and then the whole
//     offscreen area is copied to the current view --mehringe@isi.edu
//   - render is in a sub class of View
//---------------------------------------------------------------------
void
View::draw() {
	if (offscreen_ == 0) {
		return;
	}
	
	// Clear offscreen bitmap
	XFillRectangle(Tk_Display(tk_), offscreen_, background_,
	               0, 0, width_, height_);

	// Draw objects onto offscreen bitmap
	render();
	
	// Copy over offscreen bitmap to current view
	XCopyArea(Tk_Display(tk_), offscreen_, Tk_WindowId(tk_), background_,
	          0, 0, width_, height_, 0, 0);
}

//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
View::View(const char* name, int aspect, int width, int height) :
	next_(NULL),
	magnification_(1.0),
	panx_(0.0),
	pany_(0.0), 
	aspect_(aspect),
	xscroll_(NULL),
	yscroll_(NULL),
	bClip_(0) {

	Tcl& tcl = Tcl::instance();
	tk_ = Tk_CreateWindowFromPath(tcl.interp(), tcl.tkmain(),
	                              (char *) name, 0);

	if (tk_) {
		Tk_SetClass(tk_, "NetView");
		/*XXX*/
		/* Specify preferred window size. */
		Tk_GeometryRequest(tk_, width, height);

		Tk_CreateEventHandler(tk_, ExposureMask|StructureNotifyMask,
		                      handle, (ClientData)this);

		Tk_MakeWindowExist(tk_);

		pixelsPerMM_ = WidthOfScreen(Tk_Screen(tk_)) / 
		               WidthMMOfScreen(Tk_Screen(tk_));
	} else {
		// Just an arbitrary initialization
		pixelsPerMM_ = 1;
	}

	width_ = height_ = 0;
	background_ = Paint::instance()->background_gc();
	offscreen_ = 0;
	load_fonts();
}



View::~View()
{
	/* 
	 * Do not do Tk_DestroyWindow() stuff. Should *never* use
	 * delete <view> to destroy a view
	 */ 
	if (tk_ != 0) {
		if (offscreen_ != 0)
			Tk_FreePixmap(Tk_Display(tk_), offscreen_);
		free_fonts();
	}
	tk_ = 0;
}

//----------------------------------------------------------------------
// void
// View::BoundingBox(BBox& destination)
//  
//----------------------------------------------------------------------
void
View::BoundingBox(BBox& destination) {
  fprintf(stderr,"View::BoundingBox\n");
}

void View::setClipRect(BBox &b) 
{
	XRectangle rect;
	Paint* paint = Paint::instance();
	clip_ = b;
	bClip_ = 1;
	clip_.xmin = clip_.ymin = 0;
	clip_.xmax = (float)width_;
	clip_.ymax = (float)height_;
	b.xrect(rect);
#ifndef WIN32
	for (int i = 0; i < paint->num_gc(); i++) {
		XSetClipRectangles(Tk_Display(tk_), paint->paint_to_gc(i),
				   0, 0, &rect, 1, Unsorted);
	}
#endif
}

void View::clearClipRect()
{
	Paint* paint = Paint::instance();
#ifndef WIN32
	for (int i = 0; i < paint->num_gc(); i++) {
		// XXX Or should I set it to the whole window???
		XSetClipRectangles(Tk_Display(tk_), paint->paint_to_gc(i),
				   0, 0, None, 1, Unsorted);
	}
#endif
	bClip_ = 0;
}

void View::setFunction(int func)
{
	Paint* paint = Paint::instance();
	for (int i = 0; i < paint->num_gc(); i++) 
		XSetFunction(Tk_Display(tk_), paint->paint_to_gc(i), func);
}

/* Handler for the Expose, DestroyNotify and ConfigureNotify events. */
void View::handle(ClientData cd, XEvent* ep)
{
	View* nv = (View*)cd;
	
	switch (ep->type) {
	case Expose:
		if (ep->xexpose.count == 0)
			/*XXX*/
			nv->draw();
		break;
		
	case DestroyNotify:
		/*XXX kill ourself */
		/*XXX: this should kill the viewer! */
		/*XXX how about use delete this? */
		if (nv->tk_ != 0) {
			if (nv->offscreen_ != 0)
				Tk_FreePixmap(Tk_Display(nv->tk_), 
					      nv->offscreen_);
			nv->free_fonts();
		}
		nv->tk_ = 0;
		delete nv;
		break;
		
	case ConfigureNotify:
		if (nv->width_ != ep->xconfigure.width ||
		    nv->height_ != ep->xconfigure.height) {
		  //if it gets smaller, there will be no expose event,
		  //so we have to draw it outselves - mjh
		  int smaller=0;
		  if ((nv->width_ >= ep->xconfigure.width) &&
		      (nv->height_ >= ep->xconfigure.height))
		    smaller=1;
		  nv->resize(ep->xconfigure.width,
			     ep->xconfigure.height);
		  if (smaller)
		    nv->draw();
		}
		break;
	}
}

int View::command(ClientData cd, Tcl_Interp* tcl, int argc, CONST84 char **argv)
{
  NetView *nv = (NetView *)cd;
  if (argc < 2) {
    Tcl_AppendResult(tcl, "\"", argv[0], "\": arg mismatch", 0);
    return (TCL_ERROR);
  }
  if (strcmp(argv[1], "xscroll") == 0) {
    if (argc == 3) {
      nv->xscroll_=new char[strlen(argv[2])+1];
      strcpy(nv->xscroll_, argv[2]);
      return TCL_OK;
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
                       "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  if (strcmp(argv[1], "yscroll") == 0) {
    if (argc == 3) {
      nv->yscroll_=new char[strlen(argv[2])+1];
      strcpy(nv->yscroll_, argv[2]);
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
	nv->panx_+=(1.0-1.0/mag)/(2.0*nv->magnification_);
	nv->pany_+=(1.0-1.0/mag)/(2.0*nv->magnification_);
      } else {
	nv->panx_-=(1.0-mag)/(2.0*nv->magnification_*mag);
	nv->pany_-=(1.0-mag)/(2.0*nv->magnification_*mag);
      }
      nv->zoom(mag);
      nv->draw();
      //nv->pan(panx, pany);
      return TCL_OK;
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
		       "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  if ((strcmp(argv[1], "xview") == 0) ||
      (strcmp(argv[1], "yview") == 0)) {
    if ((argc==4) && (strcmp(argv[2], "moveto")==0)) {
      if (strcmp(argv[1], "xview") == 0)
	nv->panx_=atof(argv[3]);
      else
	nv->pany_=atof(argv[3]);
      nv->resize(nv->width_, nv->height_);
      nv->draw();

    } else if ((argc==5) && (strcmp(argv[2], "scroll")==0)) {
      float step = atof(argv[3]);

      if (strcmp(argv[4], "units")==0) {
	step *= 0.05/nv->magnification_;
      } else if (strcmp(argv[4], "pages")==0) {
	step *= 0.8/nv->magnification_;
      } else if (strcmp(argv[4], "world")==0) {
        // Used for canvas grab style dragging
       // float x0, y0, x_step, y_step;
       // nv->matrix_.map(0.0, 0.0, x0, y0);
      //  if (strcmp(argv[1], "xview") == 0) {
      //    nv->matrix_.map(step, 0.0, x_step, y_step);
      //    step = x0 - x_step;
      //  } else {
      //    nv->matrix_.map(step, 0.0, x_step, y_step);
      //    step = y0 - y_step;
      //  }
        step /= nv->magnification_;
      }  

      if (strcmp(argv[1], "xview") == 0)
	nv->panx_ += step;
      else
	nv->pany_ += step;

    //  fprintf(stderr, "View::command pan %f,%f\n",nv->panx_,nv->pany_);

      nv->resize(nv->width_, nv->height_);
      nv->draw();

    } else if ((argc == 7) && (strcmp(argv[2], "grab") == 0)) {
      float x_start, y_start;
      float world_x_start, world_y_start;

      nv->matrix_.imap(atof(argv[3]), atof(argv[4]),
                       world_x_start, world_y_start); 
      x_start = atof(argv[3]);
      y_start = atof(argv[3]);


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
View::line(float x0, float y0, float x1, float y1, int paint)
{
	int ax, ay;
	matrix_.map(x0, y0, ax, ay);
	int bx, by;
	matrix_.map(x1, y1, bx, by);
	GC gc = Paint::instance()->paint_to_gc(paint);
	XDrawLine(Tk_Display(tk_), offscreen_, gc, ax, ay, bx, by);
}

void View::rect(float x0, float y0, float x1, float y1, int paint)
{
	int x, y;
	matrix_.map(x0, y0, x, y);
	int xx, yy;
	matrix_.map(x1, y1, xx, yy);
	
	int w = xx - x;
	if (w < 0) {
		x = xx;
		w = -w;
	}
	int h = yy - y;
	if (h < 0) {
		h = -h;
		y = yy;
	}
	GC gc = Paint::instance()->paint_to_gc(paint);
	XDrawRectangle(Tk_Display(tk_), offscreen_, gc, x, y, w, h);
}

void View::polygon(const float* x, const float* y, int n, int paint)
{
	/*XXX*/
	XPoint pts[10];
	
	for (int i = 0; i < n; ++i) {
		float tx, ty;
		matrix_.map(x[i], y[i], tx, ty);
		pts[i].x = int(tx);
		pts[i].y = int(ty);
	}
	pts[n] = pts[0];
	GC gc = Paint::instance()->paint_to_gc(paint);
	XDrawLines(Tk_Display(tk_), offscreen_, gc, pts, n + 1,
		   CoordModeOrigin);
}

void View::fill(const float* x, const float* y, int n, int paint)
{
	/*XXX*/
	XPoint pts[10];
	
	for (int i = 0; i < n; ++i) {
		float tx, ty;
		matrix_.map(x[i], y[i], tx, ty);
		pts[i].x = int(tx);
		pts[i].y = int(ty);
	}
	pts[n] = pts[0];
	GC gc = Paint::instance()->paint_to_gc(paint);
	XFillPolygon(Tk_Display(tk_), offscreen_, gc, pts, n + 1,
		     Convex, CoordModeOrigin);
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
void View::circle(float x, float y, float r, int paint) {
  int tx, ty;     // Translated x and y
  int tr, dummy;  // Translated radius
  
  // Map world coordinates to current view coordinates
  matrix_.map(x, y, tx, ty);
  matrix_.map(x + r, y, tr, dummy);

//  fprintf(stderr, "View::circle x %f y %f tx %d ty %d\n", x,y,tx,ty);

  // XDrawArc starts from the corner so we have to 
  // move the tx and ty coordinates from the center
  // of the circle to the corner
  tr -= tx;
  tx -= tr;
  ty -= tr;
  
  // We want tr to be the diameter of the arc
  tr *= 2;
  GC gc = Paint::instance()->paint_to_gc(paint);
  XDrawArc(Tk_Display(tk_), offscreen_, gc, tx, ty, tr, tr, 0, 64 * 360);
}

//----------------------------------------------------------------------
// void 
// View::load_fonts()
//   - Set the font structures using values in 'fontName'(defined above)
//----------------------------------------------------------------------
void 
View::load_fonts() {
	Tcl_Interp* tcl;
	nfont_ = 0;
	
	if (tk_) {
		tcl = Tcl::instance().interp();
		// Load each font described in fontName[]
		for (int i = 0; i < NFONT; ++i) {
			fonts_[nfont_] = Tk_GetFont(tcl, tk_, fontName[i]);
			if (fonts_[nfont_] == 0) {
				fprintf(stderr, "Unable to load font: %s\n",fontName[i]);
				continue;
			}
			font_gc_[nfont_] = Paint::instance()->text_gc(Tk_FontId(fonts_[nfont_]));
			++nfont_;
		}
		if (nfont_ == 0) {
			fprintf(stderr, "nam: warning no fonts found\n");
		} else {
			default_font_ = TIMES_14POINT;
		}
	}
}

void View::free_fonts()
{
	/*XXX Tk_FreeFontStruct*/
	for (int i = 0; i < NFONT; i++) {
		Tk_FreeFont(fonts_[i]);
	}
}

//----------------------------------------------------------------------
// int
// View::lookup_font(int d)
//   - returns the index into the fonts_ array that is smaller in size
//     than d
//   - returns -1 if nfont_ is 0 which indicates that no fonts have been
//     loaded
//----------------------------------------------------------------------
int
View::lookup_font(int d) {
	int i = nfont_;
	while (--i > 0) {
		Tk_Font f = fonts_[i];
		Tk_FontMetrics p;
		Tk_GetFontMetrics(f, &p);
		if (d >= p.ascent + p.descent) {
			return (i);
    } 
	}
	if (nfont_) {
		return 0;
	}

	return -1;
}

//----------------------------------------------------------------------
// int
// View::getStringScreenWidth(char * text, double screen_height)
//   - Calculate the width of a string in screen coordinates based upon
//     the font size selected by the height of the string
//----------------------------------------------------------------------
int
View::getStringScreenWidth(const char * text, int screen_height) {
	int font_id;
	font_id = lookup_font(screen_height);
	if (font_id != -1) {
		return Tk_TextWidth(fonts_[lookup_font(screen_height)],
												text, strlen(text));
	} else { 
		return 0;
	}
		
}


//----------------------------------------------------------------------
// double
// View::getStringWidth(char * text, double world_height)
//   - calculates a strings width and returns it in world size
//----------------------------------------------------------------------
double
View::getStringWidth(const char * text, double world_height) {
  float x_min, x_max, y_min, y_max, discarded;
  double world_width;
  int screen_height, screen_width;
  
  matrix_.map(0.0, 0.0, discarded, y_min);
  matrix_.map(0.0, world_height, discarded, y_max);
  screen_height = (int) ceil(y_min - y_max);

  screen_width = getStringScreenWidth(text, screen_height);
  matrix_.imap(0.0, 0.0, x_min, discarded);
  matrix_.imap((float) screen_width, 0.0, x_max, discarded);
  world_width = x_max - x_min;

  return world_width;
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
int
View::getStringHeight(char * text) {
  return 0; 
}


//-----------------------------------------------------------------------
// void
// View::string(float fx, float fy, float dim, const char* s, 
//              int anchor, const char* color)
//     - This draws a string on the view
//     - Whoever wrote this code should be shot.  For anyone who is
//       working on nam in the future. Read this:
//         PUT SOME COMMENTS IN YOUR CODE!!!
//         and I don't mean comments like this is a hack or this
//         code is bad.  
//         Also, USE DESCRIPTIVE VARIBALE NAMES!!! Don't use just i or
//         f or d, dlow, dhigh or just p.  What the does that stand for?
//     - This code is driving me insane because I have to rewrite and 
//       comment everything just to understand what the hell the person 
//       before me was trying to do.    
//----------------------------------------------------------------------
void
View::string(float fx, float fy, float dim, const char* s, 
                  int anchor, const char* color) {
	if (nfont_ <= 0)
		return;
	
	int dummy;
	int dlow, dhigh;
	//Tcl& tcl = Tcl::instance();
	
	/*XXX this could be cached*/
	matrix_.map(0., 0., dummy, dlow);
	matrix_.map(0., 0.9 * dim, dummy, dhigh);
	int d = dhigh - dlow;
	if (d < 0)
		d = -d;
	int font = lookup_font(d);
	Tk_Font f = fonts_[font];
	Tk_FontMetrics p;
	Tk_GetFontMetrics(f, &p);

	int h = p.ascent;
	int len = strlen(s);
	int w = Tk_TextWidth(f, s, len);
	
	int x, y;
	matrix_.map(fx, fy, x, y);
	
	/* XXX still need to adjust for mismatch between d and actual height*/
	
	switch (anchor) {
		
	case ANCHOR_CENTER:
		x -= w / 2;
		y += h / 2;
		break;
		
	case ANCHOR_NORTH:
		x -= w / 2;
		y += d;
		break;
		
	case ANCHOR_SOUTH:
		x -= w / 2;
		y -= h/2;
		break;
		
	case ANCHOR_WEST:
		y += h / 2;
		break;
		
	case ANCHOR_EAST:
		x -= w;
		y += h / 2;
		break;
	}

	// XXX Very very very bad hack. Should go through Paint::text_gc()!!
	//Colormap cmap;
	//Tk_Uid colorname;
	GC fgc = font_gc_[font]; // Font gc
	if (color != NULL) {
		fgc = Paint::instance()->text_gc(Tk_FontId(fonts_[font]), color);
	}

	Tk_DrawChars(Tk_Display(tk_), offscreen_, fgc, f, s, len, x, y);
	if (color != NULL) {
		Tk_FreeGC(Tk_Display(tk_), fgc);
	}
	// XXX We don't free the color allocated above. Hope it won't 
	// be a problem!
}


//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
int
View::string(const char * text, double world_x, double world_y,
             double size, const char * color) {
  int screen_x, screen_y, font_index;
  Tk_Font font;
  Tk_FontMetrics font_metrics;
  int width;
  GC font_gc;
  int dummy, y_min, y_max;

  width = 0;
  if (nfont_ > 0) {
    matrix_.map(world_x, world_y, screen_x, screen_y);

    // Find screen height of box
    matrix_.map(0., 0., dummy, y_min);
    matrix_.map(0., 0.9 * size, dummy, y_max);
    int height = y_max - y_min ;
    if (height < 0) {
      height = -1*height;
    }
    font_index = lookup_font(height);

    //font_index = TIMES_14POINT;
    font = fonts_[font_index];
    Tk_GetFontMetrics(font, &font_metrics);

    font_gc = font_gc_[font_index]; // Font gc
    if (color) { 
      font_gc = Paint::instance()->text_gc(Tk_FontId(font), color);
    }

	  width = Tk_TextWidth(font, text, strlen(text));

    Tk_DrawChars(Tk_Display(tk_), offscreen_, font_gc, font, text, 
                 strlen(text), screen_x, screen_y);

    if (color) {
      Tk_FreeGC(Tk_Display(tk_), font_gc);
    }
  }
  return width;
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
View::boxedString(const char * text, double world_x, double world_y,
                  double vertical_size, int paint, 
                  const char * color) {
  float box_width, text_width, height;
  float x_padding, y_padding;

  height = 0;

  box_width = getStringWidth(text, vertical_size);
  text_width = getStringWidth(text, 0.9*vertical_size);

  x_padding = (box_width - text_width)/2.0;
  y_padding = 0.05*vertical_size;
  string(text, world_x + x_padding, world_y + y_padding, 0.9*vertical_size, color);

  rect(world_x, world_y, world_x + box_width, world_y + vertical_size, paint);  
}
  
  

