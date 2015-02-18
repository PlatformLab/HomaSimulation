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
 */

#ifdef WIN32
#include <windows.h>
#endif
#include "netview.h"
#include "psview.h"
#include "route.h"
#include "feature.h"
#include "agent.h"
#include "edge.h"
#include "node.h"
#include "monitor.h"
#include "paint.h"

Route::Route(Node *n, Edge *e, int group, int pktsrc, int negcache, int oif, 
	     double timer, double now) :
  Animation(0, 0),
  next_(0),
  edge_(e),
  node_(n),
  group_(group),
  pktsrc_(pktsrc),
  timeout_(timer),
  timeset_(now),
  anchor_(0),
  mark_(0)
{
  mode_=0;
  if (negcache==1)
    mode_|=NEG_CACHE;
  else
    mode_|=POS_CACHE;
  if (oif==1)
    mode_|=OIF;
  else
    mode_|=IIF;

  /*copy the transform matrix from the edge*/
  angle_ = e->angle();
  matrix_ = e->transform();
}

Route::~Route()
{
  if (monitor_!=NULL) {
    monitor_->delete_monitor_object(this);
  }
}

void Route::update_bb() 
{
  bb_.xmin = x_[0];
  bb_.xmax = x_[0];
  bb_.ymin = y_[0];
  bb_.ymax = y_[0];
  for(int i=1;i<npts_;i++)
    {
      if (bb_.xmin>x_[i]) bb_.xmin=x_[i];
      if (bb_.xmax<x_[i]) bb_.xmax=x_[i];
      if (bb_.ymin>y_[i]) bb_.ymin=y_[i];
      if (bb_.ymax<y_[i]) bb_.ymax=y_[i];
    }
}

// Moved to Animation::inside()
// int Route::inside(double /*now*/, float px, float py) const
// {
//   float minx, maxx, miny, maxy;
//   minx=x_[0];
//   maxx=x_[0];
//   miny=y_[0];
//   maxy=y_[0];
//   for(int i=1;i<npts_;i++)
//     {
//       if (minx>x_[i]) minx=x_[i];
//       if (maxx<x_[i]) maxx=x_[i];
//       if (miny>y_[i]) miny=y_[i];
//       if (maxy<y_[i]) maxy=y_[i];
//     }
//   return ((minx<=px)&&(maxx>=px)&&(miny<=py)&&(maxy>=py));
// }

void Route::place(double x0, double y0)
{
	int ctr = node_->no_of_routes(edge_)-1;
	place(x0,y0,ctr);
}

void Route::place(double x0, double y0, int ctr)
{

  float tx,ty;

  /*matrix was copied from the edge - don't need to set it up here*/
  //double size=edge_->size();
  double size = node_->size() * 0.2;
  double offset=size*2*(ctr);

  // XXX Make sure we have up-to-date information from the edge
  angle_ = edge_->angle();
  matrix_ = edge_->transform();

  matrix_.imap((float)x0,(float)y0, tx,ty);
  if ((mode_&POS_CACHE)>0)
    {
      if ((mode_&OIF)>0)
	{
	  matrix_.map((float)tx+offset, (float)(ty+size), x_[0], y_[0]);
	  matrix_.map((float)tx+offset, (float)(ty-size), x_[1], y_[1]);
	  matrix_.map((float)(tx+offset+size*3), (float)ty, x_[2], y_[2]);
	}
      if ((mode_&IIF)>0)
	{
	  matrix_.map((float)(tx+offset+size*2), (float)(ty+size), 
		      x_[0], y_[0]);
	  matrix_.map((float)(tx+offset+size*2), (float)(ty-size), 
		      x_[1], y_[1]);
	  matrix_.map((float)(tx+offset-size), (float)ty, x_[2], y_[2]);
	}
      npts_=3;
    }
  else
    {
      matrix_.map((float)(tx+offset), (float)(ty+size), x_[0], y_[0]);
      matrix_.map((float)(tx+offset+size), (float)(ty+size), x_[1], y_[1]);
      matrix_.map((float)(tx+offset+size), (float)(ty-size), x_[2], y_[2]);
      matrix_.map((float)(tx+offset), (float)(ty-size), x_[3], y_[3]);
      npts_=4;
    }
  update_bb(); 
}

int Route::matching_route(Edge *e, int group, int pktsrc, int oif) const
{
  if ((edge_==e)&&(group_==group)&&(pktsrc_==pktsrc))
    {
      if (((oif==1)&&((mode_&OIF)>0)) || ((oif==0)&&((mode_&IIF)>0)))
	return 1;
    }
  return 0;
}

const char* Route::info() const
{
        static char text[128];
	static char source[64];
	if (group_>=0)
	  {
	    if (pktsrc_<0)
	      strcpy(source, "*");
	    else
	      sprintf(source, "%d", pktsrc_);
	      
	    sprintf(text, 
		    "Multicast Route:\nSource:%s\nGroup:%d\nTimeout:%f",
		    source, group_, curtimeout_);
	    if ((mode_&NEG_CACHE)>0)
		strcat(text, "\nPrune Entry");
	    return (text);
	  }
	else
	  {
	    sprintf(text, "Unicast Route:\nDestination:%d\nTimeout:%f",
		    pktsrc_, curtimeout_);
	    return (text);
	  }
}

const char* Route::getname() const
{
  static char text[128];
  sprintf(text, "r");
  return (text);
}
void Route::monitor(Monitor *m, double /*now*/, char *result, int len) 
{
  monitor_=m;
  strncpy(result, info(), len);
}
 
MonState *Route::monitor_state()
{
  MonState *ms=new MonState;
  ms->type=MON_ROUTE;
  ms->route.src=pktsrc_;
  ms->route.group=group_;
  ms->route.node=node_;
  return ms;
}

void Route::draw(View *nv, double now) {
  nv->fill(x_, y_, npts_, paint_);  
  if (monitor_!=NULL)
    monitor_->draw(nv, (x_[0]+x_[2])/2, (y_[0]+y_[2])/2);
}

//void Route::draw(PSView *nv, double /*now*/) const
/*
{
  nv->fill(x_, y_, npts_, paint_);  
}
*/
void Route::update(double now)
{
  curtimeout_=timeout_+timeset_-now;
  if (curtimeout_<0) curtimeout_=0;
  /*XXX could make a zero timeout delete the route*/
  /* but this would make the tracefile unidirectional*/
}

void Route::reset(double /*now*/)
{
  node_->delete_route(this);
  delete this;
}
