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
 *
 * @(#) $Header: /cvsroot/nsnam/nam-1/graphview.h,v 1.6 2003/10/11 22:56:50 xuanc Exp $ (LBL)
 */

#ifndef nam_graphview_h
#define nam_graphview_h

#include "view.h"
#include "netgraph.h"
#include "transform.h"

class View;
class NetGraph;
struct TraceEvent;
class Tcl;
class Paint;

extern "C" {
#include <tk.h>
}

class GraphView : public View {
 public:
	GraphView(const char* name, NetGraph* ng);
	GraphView* next_;
	//void draw();
	void redrawGraph() { resize(width(), height()); }
	static int command(ClientData, Tcl_Interp*, int argc, CONST84 char **argv);
	virtual void render() {graph_->render(this);}
	virtual void BoundingBox(BBox &bb) {graph_->BoundingBox(bb);}
	virtual void getWorldBox(BBox & world_boundary);
 protected:
	NetGraph* graph_;
};

#endif
