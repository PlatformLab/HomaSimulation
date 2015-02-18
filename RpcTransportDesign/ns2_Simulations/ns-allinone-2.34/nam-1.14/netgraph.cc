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
 * @(#) $Header: /cvsroot/nsnam/nam-1/netgraph.cc,v 1.12 2000/05/18 18:06:32 klan Exp $ (LBL)
 */

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "config.h"
#include "graphview.h"
#include "netgraph.h"
#include "paint.h"
#include "sincos.h"
#include "state.h"
#include "bbox.h"

#include <float.h>

extern int lineno;
//static int next_pat;

/*XXX */

class NetworkGraphClass : public TclClass {
 public:
	NetworkGraphClass() : TclClass("NetworkGraph") {}
	TclObject* create(int /*argc*/, const char*const* /*argv*/) {
		return (new NetGraph);
	}
} networkgraph_class;

class EnergyNetworkGraphClass : public TclClass {
 public:
	EnergyNetworkGraphClass() : TclClass("EnergyNetworkGraph") {}
	TclObject* create(int /*argc*/, const char*const* /*argv*/) {
		return (new EnergyNetGraph);
	}
} energynetworkgraph_class;

class LinkNetworkGraphClass : public TclClass {
 public:
	LinkNetworkGraphClass() : TclClass("LinkNetworkGraph") {}
	TclObject* create(int /*argc*/, const char*const* /*argv*/) {
		return (new LinkNetGraph);
	}
} linknetworkgraph_class;

class FeatureNetworkGraphClass : public TclClass {
 public:
	FeatureNetworkGraphClass() : TclClass("FeatureNetworkGraph") {}
	TclObject* create(int /*argc*/, const char*const* /*argv*/) {
		return (new FeatureNetGraph);
	}
} featurenetworkgraph_class;

NetGraph::NetGraph()
	: views_(NULL), minx_(0), maxx_(0), miny_(0.0), maxy_(0.0), 
	  time_(0.0), tix_(-1)
{
	int i;
	for (i = 0; i < MAX_GRAPH; i++)
		graphdata_[i]=0.0;
	Paint *paint = Paint::instance();
        int p = paint->thin();
        ncolors_ = 8;
        paint_ = new int[ncolors_];
        for (i = 0; i < ncolors_; ++i)
                paint_[i] = p;
	paint_[1]=paint->lookup("red",1);
}

EnergyNetGraph::EnergyNetGraph()
  : NetGraph()
{
}

LinkNetGraph::LinkNetGraph()
  : NetGraph(), src_(-1), dst_(-1)
{
}

NetGraph::~NetGraph()
{
}

EnergyNetGraph::~EnergyNetGraph()
{
}

LinkNetGraph::~LinkNetGraph()
{
}

void NetGraph::update(double /*now*/)
{
}

void NetGraph::update(double /*now*/, Animation* /*a*/)
{
}

/* Reset all animations and queues to time 'now'. */
void NetGraph::reset(double /*now*/)
{
}

void NetGraph::render(GraphView* view)
{
  int i;

  if (miny_!=maxy_)
    {
      for(i=0;i<maxx_-minx_;i++)
	{
	  if (scaleddata_[i]!=0)
	    view->line(i, 0, i, scaleddata_[i], paint_[0]);
	}
      //draw the current time line
      view->line(tix_, 0, tix_, maxy_, paint_[1]);
    }
  else
    {
      view->string((double)minx_, 0.5, 0.5, "No such events found in tracefile.", ANCHOR_WEST);
    }
}
 
void NetGraph::BoundingBox(BBox& bb)
{
  int i;
  minx_=int(bb.xmin);
  maxx_=int(bb.xmax);
  int newtix=int(time_*(maxx_-minx_)/(maxtime_-mintime_));
  if (newtix!=tix_) {
    tix_=newtix;
  }
  for(i=0;i<maxx_-minx_;i++)
    {
      scaleddata_[i]=0.0;
    }
  maxy_=0.0;
  /*Note: storing the data in an array like this is quick, but introduces
    some aliasing effects in the scaling process.  I think they're 
    negligable for this purpose but if we really want to remove them
    we must store the unscaled data in a list instead*/
  for(i=0;i<MAX_GRAPH;i++)
    {
      int ix = int(((float)(i*(maxx_-minx_)))/MAX_GRAPH);
      scaleddata_[ix]+=graphdata_[i];
      if (scaleddata_[ix]>maxy_)
	maxy_=scaleddata_[ix];
    }
  if (miny_!=maxy_)
    {
      bb.ymin=miny_;
      bb.ymax=maxy_;
    }
  else 
    {
      bb.ymin=0.0;
      bb.ymax=1.0;
    }
}


/* Trace event handler. */
void NetGraph::handle(const TraceEvent& e, double /*now*/, int /*direction*/)
{
  switch (e.tt) 
    {
    case 'v':	/* 'variable' -- just execute it as a tcl command */
      break;
    case 'h':
      printf("hop\n");
      break;
    case 'a':
      printf("agent\n");
      break;
    case 'f':
      printf("feature\n");
      break;
    case 'r':
      printf("receive\n");
      break;
    case '+':
      printf("enqueue\n");
      break;
    case '-':
      printf("dequeue\n");
      break;
    case 'l':
      printf("link\n");
      break;
    case 'n':
      printf("node\n");
      break;
    case 'R':
      printf("Route\n");
      break;
    case 'd':
      printf("drop\n");
      break;
    }
}


/* Trace event handler. */
void EnergyNetGraph::handle(const TraceEvent& e, double /*now*/, int /*direction*/)
{
  switch (e.tt)
    {
    case 'g':
	  int timeix = int(((e.time-mintime_)*MAX_GRAPH)/(maxtime_-mintime_));
	  for (int i=timeix; i< MAX_GRAPH ; i++) graphdata_[i]+=1;
	  if (graphdata_[timeix]>maxy_)
	    maxy_=graphdata_[timeix];
      break;
    }
}


/* Trace event handler. */
void LinkNetGraph::handle(const TraceEvent& e, double /*now*/, int /*direction*/)
{
  switch (e.tt)
    {
    case 'h':
      if (gtype_==GR_BW) {
	if ((e.pe.src==src_) && (e.pe.dst==dst_)) {
	  int timeix = int(((e.time-mintime_)*MAX_GRAPH)/(maxtime_-mintime_));
	  graphdata_[timeix]+=e.pe.pkt.size;
	  if (graphdata_[timeix]>maxy_)
	    maxy_=graphdata_[timeix];
	}
      }
      break;
    case 'd':
      fflush(stdout);
      if (gtype_==GR_LOSS) {
	if ((e.pe.src==src_) && (e.pe.dst==dst_)) {
	  fflush(stdout);
	  int timeix = int(((e.time-mintime_)*MAX_GRAPH)/(maxtime_-mintime_));
	  graphdata_[timeix]+=1;
	  if (graphdata_[timeix]>maxy_)
	    maxy_=graphdata_[timeix];
	}
      }
      break;
    }
}

int NetGraph::command(int argc, const char *const *argv)
{
  Tcl& tcl = Tcl::instance();

  if (argc == 3) {
    if (strcmp(argv[1], "view") == 0) {
      /*
       * <net> view <viewName>
       * Create the window for the network layout/topology.
       */
      GraphView *v = new GraphView(argv[2], this);
      v->next_ = views_;
      views_ = v;
      return (TCL_OK);
    }
    if (strcmp(argv[1], "settime") == 0) {
      if (argc == 3) {
	time_=atof(argv[2]);
	GraphView *view = views_;
	int newtix=int(time_*(maxx_-minx_)/(maxtime_-mintime_));
	if (newtix!=tix_) {
	  while(view!=NULL) {
	    view->draw();
	    view=view->next_;
	  }
	  tix_=newtix;
	}
	return (TCL_OK);
      } else {
	tcl.resultf("\"%s\": arg mismatch", argv[0]);
	return (TCL_ERROR);
      }
    }
    
  }
  if (argc == 4 && strcmp(argv[1], "timerange") == 0) {
    mintime_=atof(argv[2]);
    maxtime_=atof(argv[3]);
    return (TCL_OK);
  }
  /* If no NetModel commands matched, try the Object commands. */
  return (TclObject::command(argc, argv));
}

int EnergyNetGraph::command(int argc, const char *const *argv)
{
  return (NetGraph::command(argc, argv));
}


int LinkNetGraph::command(int argc, const char *const *argv)
{
  //Tcl& tcl = Tcl::instance();
  if (argc == 4) {
//      printf("command: %s %s %s\n", argv[1],argv[2],argv[3]);
    if (strcmp(argv[1], "bw") == 0) {
      src_=atoi(argv[2]);
      dst_=atoi(argv[3]);
      gtype_=GR_BW;
      return (TCL_OK);
    }
    if (strcmp(argv[1], "loss") == 0) {
      src_=atoi(argv[2]);
      dst_=atoi(argv[3]);
      gtype_=GR_LOSS;
      return (TCL_OK);
    }
  }
  /* If no NetModel commands matched, try the Object commands. */
  return (NetGraph::command(argc, argv));
}

/************/

FeatureNetGraph::FeatureNetGraph()
  : NetGraph(), src_(-1), aname_(0), vname_(0), lastix_(0)
{
}

FeatureNetGraph::~FeatureNetGraph()
{
}

int FeatureNetGraph::command(int argc, const char *const *argv)
{

  if (argc == 5) {
    if (strcmp(argv[1], "feature") == 0) {
      src_ = atoi(argv[2]);
      aname_ = new char[strlen(argv[3]) + 1];
      strcpy(aname_, argv[3]);
      vname_ = new char[strlen(argv[4]) + 1];
      strcpy(vname_, argv[4]);
      return(TCL_OK);
    }
  }
  return (NetGraph::command(argc, argv));
}


void FeatureNetGraph::handle(const TraceEvent& e, double /*now*/, int /*direction*/)
{
  switch (e.tt) {
  case 'f':
    // print the source, the agent, name, value
    // print records we care about 
    if (src_ == e.fe.src && (strcmp(aname_, e.fe.feature.agent) == 0) &&
	(strcmp(vname_, e.fe.feature.name) == 0)) {
            /* printf("Feature %d %s %s %s\n", 
		   e.fe.src, e.fe.feature.agent, 
		   e.fe.feature.name, e.fe.feature.value); */
	    /* start out doing what is done in LinkNetGraph::handle */
	    int timeix = int(((e.time-mintime_)*MAX_GRAPH)/(maxtime_-mintime_));
	    graphdata_[timeix]+=atof(e.fe.feature.value);
	    // printf("%d %g\n", timeix, graphdata_[timeix]);
	    if (graphdata_[timeix]>maxy_)
	      maxy_=graphdata_[timeix];

	    /* fill in any points we may have missed */
	    int i;
	    double lastval = graphdata_[lastix_];
	    for (i = lastix_ + 1; i < timeix; i++) {
	      graphdata_[i] = lastval;
	    }
	    lastix_ = timeix;

    }
    break;
  default:
    break;

  }

}

/* following is a hack to get around the aliasing effects referred to
 * in the comments in NetGraph::BoundingBox.  It works for those data
 * sets I've tested it with, but it could produce undesired results
 * in some cases.
 */

void FeatureNetGraph::BoundingBox(BBox& bb)
{
  int i;
  minx_=int(bb.xmin);
  maxx_=int(bb.xmax);
  int newtix=int(time_*(maxx_-minx_)/(maxtime_-mintime_));
  if (newtix!=tix_) {
    tix_=newtix;
  }
  for(i=0;i<maxx_-minx_;i++)
    {
      scaleddata_[i]=0.0;
    }
  maxy_=1.0;
  /*Note: storing the data in an array like this is quick, but introduces
    some aliasing effects in the scaling process.  I think they're 
    negligable for this purpose but if we really want to remove them
    we must store the unscaled data in a list instead*/
  for(i=0;i<MAX_GRAPH;i++)
    {
      int ix = int(((float)(i*(maxx_-minx_)))/MAX_GRAPH);
      scaleddata_[ix]=graphdata_[i];
      if (scaleddata_[ix]>maxy_)
	maxy_=scaleddata_[ix];
    }
  if (miny_!=maxy_)
    {
      bb.ymin=miny_;
      bb.ymax=maxy_;
    }
  else 
    {
      bb.ymin=0.0;
      bb.ymax=1.0;
    }
}


