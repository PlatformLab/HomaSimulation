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
 * @(#) $Header: /cvsroot/nsnam/nam-1/graphview.cc,v 1.10 2003/10/11 22:56:50 xuanc Exp $ (LBL)
 */

#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#endif

#include <ctype.h>
#include <math.h>

#include "bbox.h"
#include "graphview.h"
#include "netgraph.h"
#include "tclcl.h"
#include "paint.h"
#include "packet.h"


GraphView::GraphView(const char* name, NetGraph* g) 
        : View(name, NONSQUARE, 30, 30), next_(NULL), graph_(g)
{
  Tcl& tcl = Tcl::instance();
  tcl.CreateCommand(Tk_PathName(View::tk()), command, (ClientData)this, 0);
}

// void GraphView::draw()
// {
// 	if (offscreen_ == 0)
// 		return;
	
// 	XFillRectangle(Tk_Display(tk_), offscreen_, background_,
// 		       0, 0, width_, height_);
// 	graph_->render(this);
// 	XCopyArea(Tk_Display(tk_), offscreen_, Tk_WindowId(tk_), background_,
// 		  0, 0, width_, height_, 0, 0);
// }

int GraphView::command(ClientData cd, Tcl_Interp* tcl, int argc, CONST84 char **argv)
{
	//GraphView *gv = (GraphView *)cd;
  if (argc < 2) {
    Tcl_AppendResult(tcl, "\"", argv[0], "\": arg mismatch", 0);
    return (TCL_ERROR);
  }

//    printf("GraphView::command: %s\n", argv[1]);
  return (View::command(cd, tcl, argc, argv));
}

void 
GraphView::getWorldBox(BBox & world_boundary) {
  graph_->BoundingBox(world_boundary);
}
