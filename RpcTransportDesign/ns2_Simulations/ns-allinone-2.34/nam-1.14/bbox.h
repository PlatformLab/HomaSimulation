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
 * @(#) $Header: /cvsroot/nsnam/nam-1/bbox.h,v 1.3 2001/02/23 05:56:50 mehringe Exp $ (LBL)
 */

#ifndef nam_bbox_h
#define nam_bbox_h

#include <X11/Xlib.h>
#include <float.h>

struct BBox {
	float xmin, ymin;
	float xmax, ymax;
	float width() const { return xmax - xmin; }
	float height() const { return ymax - ymin; }
	inline void xrect(XRectangle &r) {
		r.x = (int)xmin, r.y = (int)ymin;
		r.width = (int)(xmax - xmin);
		r.height = (int)(ymax - ymin);
	}
	inline int overlap(const BBox& b) const {
		return !((b.xmin >= xmax) || (b.xmax <= xmin) ||
			 (b.ymin >= ymax) || (b.ymax <= ymin));
	}
	inline int inside(const BBox &b) const {
		return (inside(b.xmin, b.ymin) && inside(b.xmax, b.ymax));
	}
	inline int inside(float x, float y, 
			  float xhalo = 0, float yhalo = 0) const {
		return ((x >= xmin - xhalo) && (x <= xmax + xhalo) &&
			(y >= ymin - yhalo) && (y <= ymax + yhalo));
	}
	inline void clear() { 
		xmin = ymin = FLT_MAX;
		xmax = ymax = -FLT_MAX; 
	}
	inline void move(float dx, float dy) {
		xmin += dx, xmax += dx;
		ymin += dy, ymax += dy;
	}
	inline void merge(const BBox &b) {
		if (xmin > b.xmin)
			xmin = b.xmin;
		if (ymin > b.ymin)
			ymin = b.ymin;
		if (xmax < b.xmax)
			xmax = b.xmax;
		if (ymax < b.ymax)
			ymax = b.ymax;
	}
	inline void adjust() {
		float tmp;
		if (xmin > xmax)
			tmp = xmax, xmax = xmin, xmin = tmp;
		if (ymin > ymax)
			tmp = ymax, ymax = ymin, ymin = tmp;
	}

        inline void copy(BBox & destination) {
          destination.xmin = xmin;
          destination.ymin = ymin;
          destination.xmax = xmax; 
          destination.ymax = ymax;
        } 
};

#endif

