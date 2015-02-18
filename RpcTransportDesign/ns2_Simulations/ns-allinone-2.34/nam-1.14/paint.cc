/*
 * Copyright (c) 1993 Regents of the University of California.
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
 * @(#) $Header: /cvsroot/nsnam/nam-1/paint.cc,v 1.14 2003/10/11 22:56:50 xuanc Exp $ (LBL)
 */

#ifdef WIN32
#include <windows.h>
#endif

#include "config.h"
#include "paint.h"
#include "tclcl.h"

Paint* Paint::instance_;

Paint::Paint()
{
	Tcl& tcl = Tcl::instance();
	Tk_Window tk = tcl.tkmain();
	Tk_Uid bg = Tk_GetOption(tk, "viewBackground", NULL);
	XColor* p = Tk_GetColor(tcl.interp(), tk, bg);
	if (p == 0)
		abort();
	background_ = p->pixel;
	XGCValues v;
	v.background = background_;
	v.foreground = background_;
	const unsigned long mask = GCForeground|GCBackground;

	gctab_ = new GC[PaintStaticGCSize];
	rgb_ = new rgb[PaintStaticGCSize];
	gcSize_ = PaintStaticGCSize;

	gctab_[0] = Tk_GetGC(tk, mask, &v);
	rgb_[0].colorname = strdup("black");
	rgb_[0].red=p->red;
	rgb_[0].green=p->green;
	rgb_[0].blue=p->blue;
	ngc_ = 1;

	thick_ = lookup("black", 3);
	thin_ = lookup("black", 1);
	xor_ = lookupXor("black", 1);
}

void Paint::init()
{
	instance_ = new Paint;
}

GC Paint::text_gc(Font fid, const char* color)
{
	Tcl& tcl = Tcl::instance();
	Tk_Window tk = tcl.tkmain();
	XColor *p;
	if (color == NULL)
		p = Tk_GetColor(tcl.interp(), tk, "black");
	else {
		Tk_Uid colorname;
		if (strcmp(color, "background") == 0)
			colorname = Tk_GetOption(tk, "viewBackground", NULL);
		else 
			colorname = Tk_GetUid(color);
		p = Tk_GetColor(tcl.interp(), tk, colorname);
		// Default to black
		if (p == 0)
			p = Tk_GetColor(tcl.interp(), tk, "black");
	}
	if (p == 0) {
		fprintf(stderr, "__FUNCTION__: Unknown color name %s\n", 
			color);
		abort();
	}

	XGCValues v;
	v.background = background_;
	v.foreground = p->pixel;
	v.line_width = 1;
	v.font = fid;
	const unsigned long mask = GCFont|GCForeground|GCBackground|GCLineWidth;
	return (Tk_GetGC(tk, mask, &v));
}

const char* Paint::lookupName(int r, int g, int b)
{
        XColor color;
	Tcl& tcl = Tcl::instance();
	Tk_Window tk = tcl.tkmain();

	color.red = r;
	color.green = g;
	color.blue = b;

	XColor *p = Tk_GetColorByValue(tk,&color);
	return (Tk_NameOfColor(p));

}
int Paint::lookup(const char * color, int linewidth)
{
	Tcl& tcl = Tcl::instance();
	Tk_Window tk = tcl.tkmain();
	Tk_Uid colorname;
	if (strcmp(color, "background") == 0)
		colorname = Tk_GetOption(tk, "viewBackground", NULL);
	else 
		colorname = Tk_GetUid(color);
	XColor* p = Tk_GetColor(tcl.interp(), tk, colorname);
	if (p == 0) {
		fprintf(stderr, "Nam: cannot find color %s, use default.\n", 
			color);
		return (0);
	}

	XGCValues v;
	v.background = background_;
	v.foreground = p->pixel;
	v.line_width = linewidth;
	const unsigned long mask = GCForeground|GCBackground|GCLineWidth;
	GC gc = Tk_GetGC(tk, mask, &v);
	int i;
	for (i = 0; i < ngc_; ++i)
		if (gctab_[i] == gc)
			return (i);
	gctab_[i] = gc;
	rgb_[i].colorname = strdup(color);
	rgb_[i].red=p->red;
	rgb_[i].green=p->green;
	rgb_[i].blue=p->blue;
	ngc_ = i + 1;

	if (ngc_ >= gcSize_) // PaintStaticGCSize)
		adjust();
	return (i);
}

int Paint::lookupXor(const char * color, int linewidth)
{
	Tcl& tcl = Tcl::instance();
	Tk_Window tk = tcl.tkmain();
	Tk_Uid colorname;
	if (strcmp(color, "background") == 0)
		colorname = Tk_GetOption(tk, "background", NULL);
	else 
		colorname = Tk_GetUid(color);
	XColor* p = Tk_GetColor(tcl.interp(), tk, colorname);
	if (p == 0) {
		fprintf(stderr, "Nam: cannot find color %s, use default.\n", 
			color);
		return (0);
	}

	XGCValues v;
	v.background = background_;
	v.foreground = p->pixel ^ background_;
	v.line_width = linewidth;
	v.function = GXxor;
	const unsigned long mask = GCForeground|GCBackground|GCLineWidth|GCFunction;
	GC gc = Tk_GetGC(tk, mask, &v);
	int i;
	for (i = 0; i < ngc_; ++i)
		if (gctab_[i] == gc)
			return (i);
	gctab_[i] = gc;
	rgb_[i].colorname = strdup(color);
	rgb_[i].red=p->red;
	rgb_[i].green=p->green;
	rgb_[i].blue=p->blue;
	ngc_ = i + 1;

	if (ngc_ >= gcSize_) // PaintStaticGCSize)
		adjust();
	return (i);
}

void Paint::adjust()
{
	GC *tg = new GC[ngc_ + PaintGCIncrement];
	rgb *tr = new rgb[ngc_ + PaintGCIncrement];
	gcSize_ = ngc_ + PaintGCIncrement;
	memcpy((char *)tg, (char *)gctab_, sizeof(GC)*ngc_);
	memcpy((char *)tr, (char *)rgb_, sizeof(rgb)*ngc_);
	delete gctab_;
	delete rgb_;
	gctab_ = tg;
	rgb_ = tr;
}

