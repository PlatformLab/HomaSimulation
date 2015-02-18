/*
 * Network model with automatic layout
 *
 * Layout code from nem by Elan Amir
 *
 * $Header: /cvsroot/nsnam/nam-1/anetmodel.cc,v 1.25 2000/11/12 23:19:39 mehringe Exp $
 */

#include <stdlib.h>
#include <math.h>
#include <float.h>

#include "random.h"
#include "view.h"
#include "netview.h"
#include "animation.h"
#include "queue.h"
#include "edge.h"
#include "node.h"
#include "agent.h"
#include "sincos.h"
#include "state.h"
#include "packet.h"
#include "anetmodel.h"

class AutoNetworkModelClass : public TclClass {
public:
	AutoNetworkModelClass() : TclClass("NetworkModel/Auto") {}
	TclObject* create(int argc, const char*const* argv) {
		if (argc < 5) 
			return 0;
		return (new AutoNetModel(argv[4]));
	}
} autonetworkmodel_class;

int AutoNetModel::AUTO_ITERATIONS_ = 1;
int AutoNetModel::INCR_ITERATIONS_ = 200;
int AutoNetModel::RANDOM_SEED_ = 1;
int AutoNetModel::recalc_ = 1;
double AutoNetModel::INIT_TEMP_ = 0.30; // follow Elan's 
double AutoNetModel::MINX_ = 0.0; 
double AutoNetModel::MAXX_ = 1.0;
double AutoNetModel::MINY_ = 0.0;
double AutoNetModel::MAXY_ = 1.0;

AutoNetModel::AutoNetModel(const char *animator) : NetModel(animator)
{
	iterations_ = AUTO_ITERATIONS_;
	bind("KCa_", &kca_);
	bind("KCr_", &kcr_);
	bind("Recalc_", &recalc_);
	bind("INCR_ITERATIONS_", &INCR_ITERATIONS_);
	bind("AUTO_ITERATIONS_", &AUTO_ITERATIONS_);
	bind("RANDOM_SEED_", &RANDOM_SEED_);
}

AutoNetModel::~AutoNetModel()
{
}

int AutoNetModel::command(int argc, const char *const *argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "layout") == 0) {
			/*
			 * <net> layout
			 * compute layout using Elan's nem
			 */
			layout();
			return (TCL_OK);
		}
		if (strcmp(argv[1], "relayout") == 0) {
			relayout();
			return (TCL_OK);
		}
		if (strcmp(argv[1], "reset") == 0) {
			reset_layout();
			return (TCL_OK);
		}
	} 
	return (NetModel::command(argc, argv));
}

void AutoNetModel::reset_layout() 
{
	Node *n;
	Edge *e;

	initEmbedding();
	for (n = nodes_; n != 0; n = n->next_)
		for (e = n->links(); e != 0; e = e->next_)
			e->unmark();
	scale_estimate();
	placeEverything();
	for (View *p = views_; p != 0; p = p->next_)
		if ((p->width() > 0) && (p->height() > 0)) {
			p->redrawModel();
			p->draw();
		}
}

void AutoNetModel::initEmbedding() 
{
	Node *n;
	double maxx, maxy;

	Random::seed_heuristically();

	maxx = MINX_ + (MAXX_-MINX_) * 0.4;
	maxy = MINY_ + (MAXY_-MINY_) * 0.4;	

	// randomize nodes' position within square [(0,0), (1,1)]
	for (nNodes_ = 0, n = nodes_; n != 0; n = n->next_, nNodes_++) {
		n->place(Random::uniform(MINX_, maxx),
			 Random::uniform(MINY_, maxy));
	}
	//optk_ = kc_ * sqrt((MAXX_-MINX_)*(MAXY_-MINY_) / (double)nNodes);
	optka_ = kca_ * sqrt((MAXX_-MINX_)*(MAXY_-MINY_) / (double)nNodes_);
	optkr_ = kcr_ * sqrt((MAXX_-MINX_)*(MAXY_-MINY_) / (double)nNodes_);
	temp_ = INIT_TEMP_;
}

void AutoNetModel::placeEverything()
{
	Node *n; 
	// Should re-initialize nymin_ and nymax_
	nymin_ = FLT_MAX;
	nymax_ = -FLT_MAX;
	for (n = nodes_; n != 0; n = n->next_) {
		for (Edge *e = n->links(); e != 0; e = e->next_)
			placeEdge(e, n);
		placeAllAgents(n);
		if (nymin_ > n->y())
			nymin_ = n->y();
		if (nymax_ < n->y())
			nymax_ = n->y();
	}
	// Place all routes
	for (n = nodes_; n != 0; n = n->next_) {
		n->clear_routes();
		n->place_all_routes();
	}
}

// do more passes
void AutoNetModel::relayout()
{
	Node *n;
	Edge *e;
	temp_ = INIT_TEMP_;
	for (n = nodes_; n != 0; n = n->next_)
		for (e = n->links(); e != 0; e = e->next_)
			e->unmark();

	//weigh_subtrees();
	//bifucate_graph();

	// in case that the two constants changed
	optka_ = kca_ * sqrt((MAXX_-MINX_)*(MAXY_-MINY_) / (double)nNodes_);
	optkr_ = kcr_ * sqrt((MAXX_-MINX_)*(MAXY_-MINY_) / (double)nNodes_);

	for (int i = 0; i < 100; i++) {
	  //alternate doses of vigorous shaking and letting it settle a little
          //seem to work best.
	        if ((i==0)||(i==60)) temp_ = INIT_TEMP_;
	        if ((i==50)||(i==70)) temp_ = INIT_TEMP_/4.0;

	        if ((i<50)||((i>=60)&&(i<70)))
		{
		  embed_phase1();
		}
		else
		{
		  embed_phase2();
		}
		for (n = nodes_; n != 0; n = n->next_)
			for (e = n->links(); e != 0; e = e->next_)
				e->unmark();
		scale_estimate();
		placeEverything();
		for (View *p = views_; p != 0; p = p->next_)
			if ((p->width() > 0) && (p->height() > 0)) {
				// XXX assume this means tk has been 
				// initialized
				p->redrawModel();
				p->draw();
			}
	}

}

// do several passes of embedding
void AutoNetModel::layout()
{
	initEmbedding();
	for (int i = 0; i < iterations_; i++) {
		embed_phase2();
	}

	scale_estimate(); // find node size after layout
	placeEverything();
}

void AutoNetModel::cool()
{
	if (temp_ > 0.001)
		temp_ *= 0.95;
	else 
		temp_ = 0.001;
}

void AutoNetModel::placeAllAgents(Node *src) const
{
	// clear all marks and clear all angles
	for (Agent *a = src->agents(); a != NULL; a = a->next()) {
		a->mark(0), a->angle(NO_ANGLE);
		placeAgent(a, src);
	}
}

// we need to use edge length, instead of delay, to estimate scale
void AutoNetModel::scale_estimate()
{
	/* Determine the maximum link delay. */
	double emax = 0., emean = 0., dist;
	Node *n;
	int edges=0;
	for (n = nodes_; n != 0; n = n->next_) {
		for (Edge* e = n->links(); e != 0; e = e->next_) {
		        edges++;
			dist = n->distance(*(e->neighbor()));
			emean+=dist;
			if (dist > emax)
				emax = dist;
		}
	}
	emean/=edges;
	
	/*store this because we need it for monitors*/
	node_size_ = node_sizefac_ * emean;
	/*
	 * Check for missing node or edge sizes. If any are found,
	 * compute a reasonable default based on the maximum edge
	 * dimension.
	 */
	for (n = nodes_; n != 0; n = n->next_) {
		n->size(node_size_);
		for (Edge* e = n->links(); e != 0; e = e->next_)
			e->size(.03 * emean);
	}
}

// Fruchterman, T.M.J and Reingold, E.M.,
// Graph Drawing by Force-directed Placement,
// Software-Practice and Experience, vol. 21(11), 1129-1164 (Nov. 91)
//
// Do one pass of embedding
void AutoNetModel::embed_phase1()
{
	Node *v, *u;	
	Edge* e;
	double dx, dy, mag, f, minn, rx, ry;
	/*
	 * Randomly jitter everything to try and break out of local minima
         */
	for (v = nodes_; v != 0; v = v->next_) {
	  v->displace(0., 0.);
	  rx=0.1*(MAXX_-MINX_)*
	    ((INIT_TEMP_-temp_)/INIT_TEMP_)*
	    ((Random::random()&0xffff)/32768.0 - 1.0);
	  ry=0.1*(MAXX_-MINX_)*
	    ((INIT_TEMP_-temp_)/INIT_TEMP_)*
	    ((Random::random()&0xffff)/32768.0 - 1.0);
	  v->displace(rx,ry);
	}
	/*
	 * Calculate repulsive forces between all vertices.
	 */
	for (v = nodes_; v != 0; v = v->next_) {
		for (u = nodes_; u != 0; u = u->next_) {
			if (u == v) 
				continue;
			dx = v->x() - u->x();
			dy = v->y() - u->y();
			if (dx == 0 && dy == 0)
				dx = dy = 0.001;
			mag = sqrt(dx * dx + dy * dy);
			f = REP(mag, optkr_);
			v->displace(v->dx() + ((dx / mag) * f),
				    v->dy() + ((dy / mag) * f));
		}
	}
	/* 
	 * Limit the maximum displacement to the temperature temp;
	 * and then prevent from being displaced outside frame.
	 */
	if (recalc_==1) {
	  for (v = nodes_; v != 0; v = v->next_) {
	    mag = sqrt(v->dx()*v->dx() + v->dy() * v->dy());
	    double posx = v->x(), posy = v->y();
	    if (mag != 0) {
	      minn = min(mag, temp_);
	      posx += v->dx() / mag * minn;
	      posy += v->dy() / mag * minn;
	    }
	    
	    v->place(posx, posy);
	    v->displace(0.,0.);
	  }
	}
	/*
	 * Calculate attractive forces between neighbors.
	 */
	for (v = nodes_; v != 0; v = v->next_) {
		for (e = v->links(); e != 0; e = e->next_) {
			u = e->neighbor();
			dx = v->x() - u->x();
			dy = v->y() - u->y();
			mag = sqrt(dx * dx + dy * dy);
			// XXX we consider single direction edge as 
			// of single direction attraction. So we only
			// attract one node toward the other. If later 
			// we have the other edge, the other node will be 
			// attracted too.
			if (mag >= 0) {
				f = ATT(mag, optka_);
				v->displace(v->dx() - dx / mag * f,
					    v->dy() - dy / mag * f);
			}
		}
	}
	/* 
	 * Limit the maximum displacement to the temperature temp;
	 * and then prevent from being displaced outside frame.
	 */
	for (v = nodes_; v != 0; v = v->next_) {
		mag = sqrt(v->dx()*v->dx() + v->dy() * v->dy());
		double posx = v->x(), posy = v->y();
		if (mag != 0) {
			minn = min(mag, temp_);
			posx += v->dx() / mag * minn;
			posy += v->dy() / mag * minn;
		}

#if 0
		posx = min(MAXX_, max(MINX_, posx));
		posy = min(MAXY_, max(MINY_, posy));
#endif

		v->place(posx, posy);
#if 0
		printf("Position of node %s: (%f, %f)\n", 
		       v->name(), posx, posy);
#endif
	}

	/* Cool the temperature */
	if (temp_ > 0.001)
		temp_ *= 0.95;
	else 
		temp_ = 0.001;

#if 0
	printf("------------------------------\n");
#endif
}

void AutoNetModel::embed_phase2()
{
	Node *v, *u;	
	Edge* e;
	double dx, dy, mag, f, minn;
	/*
	 * Calculate repulsive forces between all vertices.
	 */
	for (v = nodes_; v != 0; v = v->next_) {
		//XXX why not 0. as in paper?
	        v->displace(0.,0.);
		for (u = nodes_; u != 0; u = u->next_) {
			if (u == v) 
				continue;
			dx = v->x() - u->x();
			dy = v->y() - u->y();
			if (dx == 0 && dy == 0)
				dx = dy = 0.001;
			mag = sqrt(dx * dx + dy * dy);
			f = REP(mag, optkr_);
			if (f < 2*u->size()) {
			  if (f<temp_)
			  {
			    f=2*u->size()/(mag/* *temp_*/);
			  }
			  else 
			    f=2*u->size()/mag;
			}
			v->displace(v->dx() + ((dx / mag) * f),
				    v->dy() + ((dy / mag) * f));
		}
	}
	/* 
	 * Limit the maximum displacement to the temperature temp;
	 * and then prevent from being displaced outside frame.
	 */
	if (recalc_==1) {
	  for (v = nodes_; v != 0; v = v->next_) {
	    mag = sqrt(v->dx()*v->dx() + v->dy() * v->dy());
	    double posx = v->x(), posy = v->y();
	    if (mag != 0) {
	      minn = min(mag, temp_);
	      posx += v->dx() / mag * minn;
	      posy += v->dy() / mag * minn;
	    }
	    
	    v->place(posx, posy);
	    v->displace(0.,0.);
	  }
	}
	/*
	 * Calculate attractive forces between neighbors.
	 */
	for (v = nodes_; v != 0; v = v->next_) {
	  //if(v->mass()<=0) continue;
		for (e = v->links(); e != 0; e = e->next_) {
			u = e->neighbor();
			//if(u->mass()<=0) continue;
			dx = v->x() - u->x();
			dy = v->y() - u->y();
			mag = sqrt(dx * dx + dy * dy);
			// XXX we consider single direction edge as 
			// of single direction attraction. So we only
			// attract one node toward the other. If later 
			// we have the other edge, the other node will be 
			// attracted too.
			if (mag >= ((INIT_TEMP_-temp_)/INIT_TEMP_)*(e->length()+(v->size()+u->size())*0.75)) {
				f = ATT2(mag, optka_);
				v->displace(v->dx() - dx / mag * f,
					    v->dy() - dy / mag * f);
			}
		}
	}
	/* 
	 * Limit the maximum displacement to the temperature temp;
	 * and then prevent from being displaced outside frame.
	 */
	for (v = nodes_; v != 0; v = v->next_) {
	  //if(v->mass()<=0) continue;
		mag = sqrt(v->dx()*v->dx() + v->dy() * v->dy());
		double posx = v->x(), posy = v->y();
		if (mag != 0) {
			minn = min(mag, temp_);
			posx += v->dx() * minn / mag;
			posy += v->dy() * minn / mag;
		}

#if 0
		posx = min(MAXX_, max(MINX_, posx));
		posy = min(MAXY_, max(MINY_, posy));
#endif

		v->place(posx, posy);
#if 0
		printf("Position of node %s: (%f, %f)\n", 
		       v->name(), posx, posy);
#endif
	}

	/* Cool the temperature */
        if (temp_ > 0.001)
                temp_ *= 0.95;
        else
                temp_ = 0.001;

#if 0
	printf("------------------------------\n");
#endif
}


// packet handling stuff. use new packet
void AutoNetModel::handle(const TraceEvent& e, double now, int direction)
{
	switch (e.tt) {
	case 'a':
	{
		NetModel::handle(e, now, direction);
		// recalculate bounding box so that all agents will be within
		// view
		for (View *v = views_; v != 0; v = v->next_)
			v->redrawModel();
		break;
	}
	default:
		NetModel::handle(e, now, direction);
		break;
	}
}

void AutoNetModel::bifucate_graph()
{
    Node *n, *m;
    for (n = nodes_; n != 0; n = n->next_) {
      if (n->mass()<=0) continue;
      int remaining=0;
      for (m = nodes_; m != 0; m = m->next_) {
	if (m->mass()>0) remaining++;
	m->mark(0);
      }
      int depth=0;
      int result=2;
      while((result!=0)&&(result!=1))
      {
	depth++;
	for (m = nodes_; m != 0; m = m->next_) {
	  if (m->mass()>0) {
	    m->mark(0);
	    m->color("black");
	  }
	}
	result=mark_to_depth(n, depth);
	printf("depth: %d result: %d\n", depth, result);
      }
      if (result==1) {
	int ctr=0;
	for (m = nodes_; m != 0; m = m->next_) {
	  if (m->marked()==1) ctr++;
	}
	printf("remaining: %d ctr: %d\n", remaining, ctr);
	if ((remaining-ctr>1)&&(ctr>1)&&(ctr<remaining/2))
	{
	  printf("success!\n");
	  for (m = nodes_; m != 0; m = m->next_) {
	    if (m->marked()==1)
	    {
	      m->mass(-1);
	      m->color("blue");
	    }
	  }
	}
      } else {
	printf("failed at depth %d!\n", depth);
      }
    }
}

int AutoNetModel::mark_to_depth(Node *n, int depth)
{
  int sum=0;
  if (depth==0) {
    n->mark(2);
    return 1;
  }
  if (n->marked()==2) sum--;
  n->mark(1);


  for(Edge *e = n->links(); e != 0; e = e->next_)
  {
    Node *dst=e->neighbor();
    if ((dst->mass()>0)&&(dst->marked()==0))
      sum+=mark_to_depth(dst, depth-1);
  }
  return sum;
}

void AutoNetModel::weigh_subtrees()
{
  Node *n, *dst, *newdst;
  Edge* e;
  int nodes=0;
  for (n = nodes_; n != 0; n = n->next_) {
    n->mass(1);
  }
  for (n = nodes_; n != 0; n = n->next_) {
    int ctr=0;
    for (e = n->links(); e != 0; e = e->next_)
      ctr++;
    if (ctr==1)
    {
      dst=n->links()->neighbor();
      dst->mass(2);
      n->mass(0);
      n->color("green");
      nodes++;
    }
    while(ctr==1) {
      ctr=0;
      for (e = dst->links(); e != 0; e = e->next_)
	if (e->neighbor()->mass()==1) ctr++;
      if (ctr==1)
      {
	for (e = dst->links(); e != 0; e = e->next_)
	  if (e->neighbor()->mass()==1) {
	    newdst=e->neighbor();
	    newdst->mass(1+dst->mass());
	    dst->mass(0);
	    dst->color("red");
	    nodes++;
	    dst=newdst;
	    break;
	  }
      }
    }
  }
  printf("massless nodes: %d\n", nodes);
  nodes=0;
  for (n = nodes_; n != 0; n = n->next_) {
    if (n->mass()==0) {
      nodes++;
      n->color("grey");
    }
  }
  printf("massless nodes: %d\n", nodes);
}
    

void AutoNetModel::place_subtrees()
{
  Node *n, *dst;
  Edge* e;
  double angle;
  int nodes=0, did_something=1;
  double comx=0.0, comy=0.0, x, y;
  for (n = nodes_; n != 0; n = n->next_) {
    if (n->mass()!=0)
    {
      comx+=n->x();
      comy+=n->y();
      nodes++;
    }
  }
  comx/=nodes;
  comy/=nodes;
  printf("com at %.2f,%.2f\n", comx, comy);
  while (did_something) {
    did_something=0;
    for (n = nodes_; n != 0; n = n->next_) {
      if (n->mass()==0)
      {
	for (e = n->links(); e != 0; e = e->next_)
	  if (e->neighbor()->mass()>0)
	  {
	    dst=e->neighbor();
	    n->mass(1);
	    n->color("green");
	    x=dst->x();
	    y=dst->y();
	    if (y-comy!=0)
	      angle=atan((x-comx)/(y-comy));
	    else
	      angle=0;
	    if (y-comy>0)
	      angle+=M_PI;
	    n->place(x-2*n->size()*sin(angle), 
		     y-2*n->size()*cos(angle));
	    did_something=1;
	    break;
	  }
      }
    }
  }
}


//------------------------------------------------------------------
//  The following is a big hack and I hope it works
//
//  - It is being done to speed up the layout of realtime dynamic nodes
//    by only laying out that node and not the others
//  - These procedures should be integrated into the main relayout 
//    procedure when time permits 
//------------------------------------------------------------------

//------------------------------------------------------------------
//
//------------------------------------------------------------------
void AutoNetModel::relayoutNode(Node *v) {
  Node *n;
  Edge *e;
  temp_ = INIT_TEMP_;
  for (n = nodes_; n != 0; n = n->next_)
    for (e = n->links(); e != 0; e = e->next_)
      e->unmark();

	// in case that the two constants changed
	optka_ = kca_ * sqrt((MAXX_-MINX_)*(MAXY_-MINY_) / (double)nNodes_);
	optkr_ = kcr_ * sqrt((MAXX_-MINX_)*(MAXY_-MINY_) / (double)nNodes_);

  for (int i = 0; i < 100; i++) {
    // alternate doses of vigorous shaking and letting it settle a little
    // seem to work best.
    if ((i==0)||(i==60))
       temp_ = INIT_TEMP_;
    if ((i==50)||(i==70))
       temp_ = INIT_TEMP_/4.0;

    if ((i < 50) || ((i >= 60) && (i < 70))) {
      embed_phase1(v);
    } else {
		  embed_phase2(v);
		}
		for (n = nodes_; n != 0; n = n->next_)
			for (e = n->links(); e != 0; e = e->next_)
				e->unmark();
		scale_estimate();
		placeEverything();
	}
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------
void AutoNetModel::embed_phase1(Node *v) {
	Node *u;	
	Edge* e;
	double dx, dy, mag, f, minn, rx, ry;
	/*
	 * Randomly jitter everything to try and break out of local minima
   */
	v->displace(0., 0.);
	rx = 0.1 * (MAXX_-MINX_) *
      ((INIT_TEMP_-temp_)/INIT_TEMP_) *
      ((Random::random()&0xffff)/32768.0 - 1.0);
  ry = 0.1 * (MAXX_-MINX_) *
       ((INIT_TEMP_-temp_)/INIT_TEMP_) *
       ((Random::random()&0xffff)/32768.0 - 1.0);
  v->displace(rx,ry);
	/*
	 * Calculate repulsive forces between all vertices.
	 */
	for (u = nodes_; u != 0; u = u->next_) {
	   if (u == v) 
	      continue;
	   dx = v->x() - u->x();
	   dy = v->y() - u->y();
	   if (dx == 0 && dy == 0)
	     dx = dy = 0.001;
	   mag = sqrt(dx * dx + dy * dy);
	   f = REP(mag, optkr_);
     v->displace(v->dx() + ((dx / mag) * f),
                 v->dy() + ((dy / mag) * f));
	}
	/* 
	 * Limit the maximum displacement to the temperature temp;
	 * and then prevent from being displaced outside frame.
	 */
	if (recalc_ == 1) {
	   mag = sqrt(v->dx()*v->dx() + v->dy() * v->dy());
	   double posx = v->x(), posy = v->y();
	   if (mag != 0) {
	      minn = min(mag, temp_);
	      posx += v->dx() / mag * minn;
	      posy += v->dy() / mag * minn;
	   }
	    
	   v->place(posx, posy);
	   v->displace(0.,0.);
	}
	/*
	 * Calculate attractive forces between neighbors.
	 */
	for (e = v->links(); e != 0; e = e->next_) {
	   u = e->neighbor();
	   dx = v->x() - u->x();
	   dy = v->y() - u->y();
	   mag = sqrt(dx * dx + dy * dy);
	   // XXX we consider single direction edge as 
	   // of single direction attraction. So we only
	   // attract one node toward the other. If later 
	   // we have the other edge, the other node will be 
	   // attracted too.
	   if (mag >= 0) {
	      f = ATT(mag, optka_);
	      v->displace(v->dx() - dx / mag * f,
			  v->dy() - dy / mag * f);
	   }
	}
	/* 
	 * Limit the maximum displacement to the temperature temp;
	 * and then prevent from being displaced outside frame.
	 */
	mag = sqrt(v->dx()*v->dx() + v->dy() * v->dy());
	double posx = v->x(), posy = v->y();
	if (mag != 0) {
	   minn = min(mag, temp_);
	   posx += v->dx() / mag * minn;
	   posy += v->dy() / mag * minn;
	}

	v->place(posx, posy);

	/* Cool the temperature */
	if (temp_ > 0.001)
		temp_ *= 0.95;
	else 
		temp_ = 0.001;
}

//------------------------------------------------------------------
//
//------------------------------------------------------------------
void AutoNetModel::embed_phase2(Node *v) {
	Node *u;	
	Edge* e;
	double dx, dy, mag, f, minn;
	/*
	 * Calculate repulsive forces between all vertices.
	 */
	//XXX why not 0. as in paper?
	v->displace(0.,0.);
	for (u = nodes_; u != 0; u = u->next_) {
	   if (u == v) 
	      continue;
	   dx = v->x() - u->x();
	   dy = v->y() - u->y();
	   if (dx == 0 && dy == 0)
	      dx = dy = 0.001;
	   mag = sqrt(dx * dx + dy * dy);
	   f = REP(mag, optkr_);
	   if (f < 2*u->size()) {
	      if (f<temp_) {
		      f=2*u->size()/(mag/* *temp_*/);
		    } else { 
		      f=2*u->size()/mag;
        }
	   }
	   v->displace(v->dx() + ((dx / mag) * f),
		       v->dy() + ((dy / mag) * f));
	}
	/* 
	 * Limit the maximum displacement to the temperature temp;
	 * and then prevent from being displaced outside frame.
	 */
	if (recalc_==1) {
	   mag = sqrt(v->dx()*v->dx() + v->dy() * v->dy());
	   double posx = v->x(), posy = v->y();
	   if (mag != 0) {
	      minn = min(mag, temp_);
	      posx += v->dx() / mag * minn;
	      posy += v->dy() / mag * minn;
	   }
	   
	   v->place(posx, posy);
	   v->displace(0.,0.);
	}
	/*
	 * Calculate attractive forces between neighbors.
	 */
	for (e = v->links(); e != 0; e = e->next_) {
	   u = e->neighbor();
	   //if(u->mass()<=0) continue;
	   dx = v->x() - u->x();
	   dy = v->y() - u->y();
	   mag = sqrt(dx * dx + dy * dy);
	   // XXX we consider single direction edge as 
	   // of single direction attraction. So we only
	   // attract one node toward the other. If later 
	   // we have the other edge, the other node will be 
	   // attracted too.
	   if (mag >= ((INIT_TEMP_-temp_)/INIT_TEMP_) * 
                 (e->length()+(v->size()+u->size()) *
                 0.75)) {
	      f = ATT2(mag, optka_);
	      v->displace(v->dx() - dx / mag * f,
			  v->dy() - dy / mag * f);
	   }
	}
	/* 
	 * Limit the maximum displacement to the temperature temp;
	 * and then prevent from being displaced outside frame.
	 */
	mag = sqrt(v->dx()*v->dx() + v->dy() * v->dy());
	double posx = v->x(), posy = v->y();
	if (mag != 0) {
	   minn = min(mag, temp_);
	   posx += v->dx() * minn / mag;
	   posy += v->dy() * minn / mag;
	}

	v->place(posx, posy);

	/* Cool the temperature */
        if (temp_ > 0.001)
                temp_ *= 0.95;
        else
                temp_ = 0.001;

}

