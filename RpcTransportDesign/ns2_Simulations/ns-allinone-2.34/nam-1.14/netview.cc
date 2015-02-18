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
 * @(#) $Header: /cvsroot/nsnam/nam-1/netview.cc,v 1.20 2003/10/11 22:56:50 xuanc Exp $ (LBL)
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
#include "xwd.h"
#include "node.h"

int
NetView::record(const char *file)
{
#ifndef WIN32
	return xwd_Window_Dump_To_File(tk_, offscreen_, (unsigned) width_, 
				       (unsigned) height_, file);
#else
	return TCL_ERROR;
#endif
}

// Static call back function for tcl delete command callback
void NetView::DeleteCmdProc(ClientData cd)
{
	NetView *nv = (NetView *)cd;
	if (nv->tk_ != NULL) {
		Tk_DestroyWindow(nv->tk_);
	}
}

NetView::NetView(const char* name, NetModel* m) 
        : View(name, SQUARE, 300, 200), model_(m)
{
	if (tk_!=0) {
		Tcl& tcl = Tcl::instance();
		cmd_ = Tcl_CreateCommand(tcl.interp(), 
					 Tk_PathName(View::tk()),
					 command, 
					 (ClientData)this, 
					 DeleteCmdProc);
	}
}

// Do nothing. For derived classes only.
NetView::NetView(const char* name)
	: View(name, SQUARE, 300, 200), model_(NULL), cmd_(NULL)
{
}

NetView::NetView(const char* name, NetModel* m, int width, int heigth)
  : View(name, SQUARE, 300, 400), model_(NULL)
{
}

NetView::~NetView()
{
	model_->remove_view(this);
	// Delete Tcl command created
	Tcl& tcl = Tcl::instance();
	Tcl_DeleteCommandFromToken(tcl.interp(), cmd_);
}

//----------------------------------------------------------------------
// int NetView::command(ClientData cd, Tcl_Interp * tcl, 
//                      int argc, char ** argv)
//----------------------------------------------------------------------
int NetView::command(ClientData cd, Tcl_Interp * tcl, 
                     int argc, CONST84 char ** argv) {
  if (argc < 2) {
    Tcl_AppendResult(tcl, "\"", argv[0], "\": arg mismatch", 0);
    return (TCL_ERROR);
  }

  if (strcmp(argv[1], "info") == 0) {
    if (argc == 5) {
      NetView *nv = (NetView *)cd;
      Tcl& tcl = Tcl::instance();
      Animation* a;
      //double now = atof(argv[2]);
      //int rootX, rootY;
      //Window root, child;
      //unsigned int state;
      int winX, winY;
      float px, py;
       /*XQueryPointer(Tk_Display(nv->tk_),
              Tk_WindowId(nv->tk_), &root,
               &child, &rootX, &rootY, &winX, &winY,
               &state);*/
      winX=atoi(argv[3]);
      winY=atoi(argv[4]);
      nv->matrix_.imap(float(winX), float(winY), px, py);
      //if ((a = nv->model_->inside(now, px, py)) != 0) {
      if ((a = nv->model_->inside(px, py)) != 0) {
        tcl.result(a->info());
        return TCL_OK;
      }
      return TCL_OK;
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
           "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  
  if (strcmp(argv[1], "gettype")==0) {
    if (argc == 5) {
      NetView *nv = (NetView *)cd;
      Tcl& tcl = Tcl::instance();
      Animation* a;
      int winX, winY;
      float px, py;
      winX=atoi(argv[3]);
      winY=atoi(argv[4]);
      nv->matrix_.imap(float(winX), float(winY), px, py);

      if ((a = nv->model_->inside(px, py)) != 0) {
        const char * res = a->gettype();
        if (res != NULL)
          tcl.result(res);
        return TCL_OK;
      }
      return TCL_OK;
      
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
           "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  
  if (strcmp(argv[1], "getfid")==0) {
    if (argc == 5) {
      NetView *nv = (NetView *)cd;
      Tcl& tcl = Tcl::instance();
      Animation* a;
      int winX, winY;
      float px, py;
      winX=atoi(argv[3]);
      winY=atoi(argv[4]);
       nv->matrix_.imap(float(winX), float(winY), px, py);
      if ((a = nv->model_->inside(px, py)) != 0) {
        const char *res=a->getfid();
        if (res!=NULL)
          tcl.result(res);
        return TCL_OK;
      }
      return TCL_OK;
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
           "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  if (strcmp(argv[1], "getesrc")==0) {
    if (argc == 5) {
      NetView *nv = (NetView *)cd;
      Tcl& tcl = Tcl::instance();
      Animation* a;
      int winX, winY;
      float px, py;
      winX=atoi(argv[3]);
      winY=atoi(argv[4]);
       nv->matrix_.imap(float(winX), float(winY), px, py);
      if ((a = nv->model_->inside(px, py)) != 0) {
        const char *res=a->getesrc();
        if (res!=NULL)
          tcl.result(res);
        return TCL_OK;
      }
      return TCL_OK;
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
           "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  if (strcmp(argv[1], "getedst")==0) {
    if (argc == 5) {
      NetView *nv = (NetView *)cd;
      Tcl& tcl = Tcl::instance();
      Animation* a;
      int winX, winY;
      float px, py;
      winX=atoi(argv[3]);
      winY=atoi(argv[4]);
       nv->matrix_.imap(float(winX), float(winY), px, py);
      if ((a = nv->model_->inside(px, py)) != 0) {
        const char *res=a->getedst();
        if (res!=NULL)
          tcl.result(res);
        return TCL_OK;
      }
      return TCL_OK;
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
           "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  if (strcmp(argv[1], "getname")==0) {
    if (argc == 5) {
      NetView *nv = (NetView *)cd;
      Tcl& tcl = Tcl::instance();
      Animation* a;
      //double now = atof(argv[2]);
      //Window root, child;
      //int rootX, rootY; 
      //unsigned int state;
      int winX, winY;
      float px, py;
      /*XQueryPointer(Tk_Display(nv->tk_),
              Tk_WindowId(nv->tk_), &root,
               &child, &rootX, &rootY, &winX, &winY,
               &state);
              */
      winX=atoi(argv[3]);
      winY=atoi(argv[4]);
       nv->matrix_.imap(float(winX), float(winY), px, py);
      //if ((a = nv->model_->inside(now, px, py)) != 0) {
      if ((a = nv->model_->inside(px, py)) != 0) {
        const char *res=a->getname();
        if (res!=NULL)
          tcl.result(res);
        return TCL_OK;
      }
      return TCL_OK;
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
           "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  
  // Node execution script
  //   returns the script to execute if any
  //  - $netview get_node_tclscript $node_id
  if (strcmp(argv[1], "get_node_tclscript")==0) {
    if (argc == 3) {
      NetView * nv = (NetView *) cd;
      Tcl& tcl = Tcl::instance();
      Node * node = nv->model_->lookupNode(atoi(argv[2]));
      char * tcl_script = node->getTclScript();
      if (tcl_script) {
        tcl.result(tcl_script);
      }
      return TCL_OK;
      
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0], "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }

  if (strcmp(argv[1], "get_node_tclscript_label")==0) {
    if (argc == 3) {
      NetView * nv = (NetView *) cd;
      Tcl& tcl = Tcl::instance();
      Node * node = nv->model_->lookupNode(atoi(argv[2]));
      char * tcl_script = node->getTclScriptLabel();
      if (tcl_script) {
        tcl.result(tcl_script);
      }
      return TCL_OK;
      
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0], "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }

  if (strcmp(argv[1], "new_monitor") == 0) {
    if (argc == 5) {
      NetView *nv = (NetView *)cd;
      Tcl& tcl = Tcl::instance();
      Animation* a;
      //double now = atof(argv[2]);
      float px, py;
      nv->matrix_.imap(atof(argv[3]), atof(argv[4]), px, py);
      //if ((a = nv->model_->inside(now, px, py)) != 0) {
      if ((a = nv->model_->inside(px, py)) != 0) {
        char num[10];
        int n = nv->model_->add_monitor(a);
        sprintf(num, "%d", n);
        tcl.result(num);
        return TCL_OK;
      }
      return TCL_OK;
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
           "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  if (strcmp(argv[1], "delete_monitor") == 0) {
    if (argc == 3) {
      NetView *nv = (NetView *)cd;
      nv->model_->delete_monitor(atoi(argv[2]));
      return TCL_OK;
    } else {
      Tcl_AppendResult(tcl, "\"", argv[0],
           "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  if (strcmp(argv[1], "monitor") == 0) {
    if (argc == 4) {
      NetView *nv = (NetView *)cd;
      Tcl& tcl = Tcl::instance();
      //Animation* a;
      double now = atof(argv[2]);
      int monitor = atoi(argv[3]);
      char result[128];
      nv->model_->monitor(now, monitor, result, 128);
      tcl.result(result);
      return TCL_OK;
    }  else {
      Tcl_AppendResult(tcl, "\"", argv[0],
           "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  if (strcmp(argv[1], "record_frame") == 0) {
    if (argc == 3) {
      NetView *nv = (NetView *)cd;
      return nv->record(argv[2]);
    }  else {
      Tcl_AppendResult(tcl, "\"", argv[0],
           "\": arg mismatch", 0);
      return TCL_ERROR;
    }
  }
  if (strcmp(argv[1], "close") == 0) {
    NetView *nv = (NetView *)cd;
    nv->destroy();
    return TCL_OK;
  }

  return (View::command(cd, tcl, argc, argv));
}

//---------------------------------------------------------------------
// void
// NetView::render()
//   - The drawing/render sequence is a little circular.  NetModel calls
//     View::draw() which calls NetView::render which then goes back to
//     NetModel::render(View *)
//---------------------------------------------------------------------
void
NetView::render() {
  model_->render(this);
}

//---------------------------------------------------------------------
//---------------------------------------------------------------------
void
NetView::BoundingBox(BBox &bb) {
  fprintf(stderr, "NetView::BoundingBox\n");
  model_->BoundingBox(bb);
}
