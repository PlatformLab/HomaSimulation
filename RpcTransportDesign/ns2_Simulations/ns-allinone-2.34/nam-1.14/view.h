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
 * @(#) $Header: /cvsroot/nsnam/nam-1/view.h,v 1.11 2003/10/11 22:56:51 xuanc Exp $ (LBL)
 */

#ifndef nam_view_h
#define nam_view_h

#include <tk.h>
#include "tkcompat.h"

#include "netmodel.h"
#include "transform.h"
#include "bbox.h"

class NetModel;
struct TraceEvent;
class Tcl;
class Paint;

/*defines for whether aspect ratio is to be square or not*/
#define SQUARE 0
#define NONSQUARE 1

// Total number of fonts
#define NFONT 8
// Indexes into fontName and font_ arrays
#define TIMES_8POINT  0
#define TIMES_10POINT 1
#define TIMES_12POINT 2
#define TIMES_14POINT 3
#define TIMES_18POINT 4
#define TIMES_20POINT 5
#define TIMES_24POINT 6
#define TIMES_34POINT 7

// Anchor locations for use in: 
// string(float fx, float fy, float dim, const char* s, 
//        int anchor, const char * color)
#define ANCHOR_CENTER	0
#define ANCHOR_NORTH	1
#define ANCHOR_SOUTH	2
#define ANCHOR_EAST	3
#define ANCHOR_WEST	4

class View {
 public:
	View(const char* name, int aspect, int width, int height);
	virtual ~View();
	virtual void draw();
	void redrawModel() { resize(width_, height_); }

	/*
	 * Graphics interface.
	 */
	virtual void line(float x0, float y0, float x1, float y1, int color);
	virtual void rect(float x0, float y0, float x1, float y1, int color);
	virtual void polygon(const float* x, const float* y, int n, int color);
	virtual void fill(const float* x, const float* y, int n, int color);
	virtual void circle(float x, float y, float r, int color);
  int getStringScreenWidth(const char * text, int screen_height);
  double getStringWidth(const char * text, double height);
  int getStringHeight(char * text);
	virtual void string(float fx, float fy, float dim, const char* s, 
			                int anchor, const char* color = NULL);
  int string(const char * text, double world_x, double world_y,
             double size, const char * color = NULL);
  void boxedString(const char * text, double world_x, double world_y,
                   double vertical_size, int paint,
                   const char * color = NULL);
	int width() {return width_; }
	int height() {return height_; }
	Tk_Window tk() {return tk_;}

	/*
	 * Tcl command hooks.
	 */
	static int command(ClientData, Tcl_Interp*, int argc, CONST84 char **argv);
	static void handle(ClientData, XEvent*);
	void destroy() { Tk_DestroyWindow(tk_); }

	int getCoord(char *xs, char *ys, float &x, float &y);

	// Interface to transform_'s map
	void map(float& x, float& y) const { matrix_.map(x, y); }
	void imap(float& tx, float& ty) const { matrix_.imap(tx, ty); }
	
	virtual void render() {};
	virtual void render(BBox &) {};
	virtual void BoundingBox(BBox & destination); // Copy bbox into
                                                      // destination
        virtual void getWorldBox(BBox & world_boundary) = 0;

	void setClipRect(BBox &);
	void clearClipRect();
	void setFunction(int);

	View * next_;  // Next view in a list of differnet network views

 protected:
	void resize(int width, int height);
	/*
	 * transformation matrix for this view of the model.
	 */
	Transform matrix_;
	/*information for panning and zooming*/
	float magnification_;
	float panx_;
	float pany_;
	int aspect_;
	
	/*scrollbar widgets*/
	char *xscroll_;
	char *yscroll_;

	int width_;
	int height_;

	Tk_Window tk_;
	Drawable offscreen_;
	GC background_;
	void zoom(float mag);
	void pan(float x, float y);
	double pixelsPerMM_;

	/*
	 * Font structures.
	 */
	void load_fonts();
	void free_fonts();
	int lookup_font(int d);
        // Look above for index names
	Tk_Font fonts_[NFONT];
	GC font_gc_[NFONT];
	int nfont_;
        int default_font_;

        BBox canvas_clip_;

	BBox clip_;
	int bClip_;
};

#endif

