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

#ifndef nam_testview_h
#define nam_testview_h

#include "netmodel.h"
#include "transform.h"

class NetModel;
class Tcl;
class Paint;
struct TraceEvent;

extern "C" {
#include <tk.h>
}
#include "view.h"

#define SQUARE 0
#define NONSQUARE 1

class TestView : public View {
 public:
  TestView(const char *name, NetModel *m);
	TestView* next_;
	void draw();
	void line(float x0, float y0, float x1, float y1, int color);
	void rect(float x0, float y0, float x1, float y1, int color);
	void polygon(const float* x, const float* y, int n, int color);
	void fill(const float* x, const float* y, int n, int color);
	void circle(float x, float y, float r, int color);
	void string(float fx, float fy, float dim, const char* s, int anchor,
              const char* color = NULL);
	virtual void render() {} ;
	virtual void BoundingBox(BBox& bb) {model_->BoundingBox(bb);}
	virtual void getWorldBox(BBox& world_boundary);
  
  protected:
	void resize(int width, int height);
	FILE *file_;
	NetModel* model_;
	char *name_;
	float scale_;
};

#endif

