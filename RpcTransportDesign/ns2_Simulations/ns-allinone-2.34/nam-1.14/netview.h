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
 * @(#) $Header: /cvsroot/nsnam/nam-1/netview.h,v 1.14 2003/10/11 22:56:50 xuanc Exp $ (LBL)
 */

#ifndef nam_netview_h
#define nam_netview_h

extern "C" {
#include <tcl.h>
#include <tk.h>
}

#include "view.h"
#include "netmodel.h"
#include "transform.h"

class NetModel;
class View;
struct TraceEvent;
class Tcl;
class Paint;


class NetView : public View {
 public:
	NetView(const char* name, NetModel*);
	virtual ~NetView();
	//	NetView* next_;
	//	void draw();
	int record(const char *file);
	void redrawModel() { resize(width(), height()); }

	static int command(ClientData, Tcl_Interp*, int argc, CONST84 char **argv);
	virtual void render();
	virtual void BoundingBox(BBox &bb);

	static void DeleteCmdProc(ClientData cd);

 protected:
  NetView(const char* name, NetModel*, int width, int height);
	NetView(const char* name);
	NetModel* model_;
	Tcl_Command cmd_;
};

#endif

