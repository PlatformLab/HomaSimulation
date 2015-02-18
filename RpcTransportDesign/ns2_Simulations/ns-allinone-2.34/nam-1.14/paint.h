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
 * @(#) $Header: /cvsroot/nsnam/nam-1/paint.h,v 1.10 2003/10/11 22:56:50 xuanc Exp $ (LBL)
 */

#ifndef nam_paint_h
#define nam_paint_h

extern "C" {
#include <tk.h>
}

typedef struct rgb_s {
  unsigned short red, green, blue;
  char *colorname;
} rgb;

class Tcl;

const int PaintStaticGCSize = 512;
const int PaintGCIncrement = 64;

class Paint { 
 protected:
	Paint();
 public:
	static void init();
	static Paint* instance() { return (instance_); }

	inline GC paint_to_gc(int paint) { return (gctab_[paint]); }
	inline int num_gc() { return ngc_; }
	rgb *paint_to_rgb(int paint) { return (&rgb_[paint]); }
	const char *lookupName(int,int,int);
	inline GC background_gc() { return (gctab_[0]); }
	GC text_gc(Font fid, const char* color = NULL);
	int lookup(const char *color, int linewidth);
	int lookupXor(const char *color, int linewidth);
	inline int thick() { return (thick_); }
	inline int thin() { return (thin_); }
	inline int Xor() { return (xor_); }  // ansi says "xor" is a keyword
 private:
	void adjust();

	static Paint* instance_;
	int ngc_;
	int gcSize_;
	GC *gctab_;
	rgb *rgb_;
	int thick_;
	int thin_;
	int xor_;
	int background_;
};

#endif

