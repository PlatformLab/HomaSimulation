/*
 * Network model with Wireless layout
 *
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
#include "wnetmodel.h"

class WirelessNetworkModelClass : public TclClass {
public:
  WirelessNetworkModelClass() : TclClass("NetworkModel/Wireless") {}
  TclObject* create(int argc, const char*const* argv) {
    if (argc < 5) 
      return 0;
    return (new WirelessNetModel(argv[4]));
  }
} wirelessnetworkmodel_class;

WirelessNetModel::WirelessNetModel(const char *animator) :
  NetModel(animator), edges_(0) {
  bind("Wpxmax_", &pxmax_);
  bind("Wpymax_", &pymax_);
}

WirelessNetModel::~WirelessNetModel() {
}

int WirelessNetModel::command(int argc, const char *const *argv) {
	if (argc == 2) {
		if (strcmp(argv[1], "layout") == 0) {
			/*
			 * <net> layout
			 */
			layout();
			NetModel::set_wireless();
			return (TCL_OK);
		}
	} 
	if (argc == 5) {
		if (strcmp(argv[1], "select-pkt") == 0) {
			NetModel::selectPkt(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
			return (TCL_OK);
		}
	} 

	return (NetModel::command(argc, argv));
}

void WirelessNetModel::layout()
{

	Node *n;
	for (n = nodes_; n != 0; n = n->next_)
                for (Edge* e = n->links(); e != 0; e = e->next_)  
                        placeEdge(e, n); 

	nymax_ = pymax_ ;
	nymin_ = 0 ;

}

void WirelessNetModel::BoundingBox(BBox& bb)
{                       
        bb.xmin = bb.ymin = 0;
	bb.xmax = pxmax_;
	bb.ymax = pymax_;

        for (Animation* a = drawables_; a != 0; a = a->next())
                a->merge(bb);
}

void WirelessNetModel::update(double now)
{               
        Animation *a, *n;
        for (a = animations_; a != 0; a = n) {
                n = a->next();
                a->update(now);
        }

	for (Node* x = nodes_; x != 0; x = x->next_) {
		x->update(now);
		moveNode(x);
	}
        
        /*
         * Draw all animations and drawables on display to reflect
         * current time.
         */
        now_ = now;
        for (View* p = views_; p != 0; p = p->next_) {
                p->draw(); 
        }               
}       


void WirelessNetModel::moveNode(Node *n)
{

        for (Edge *e = n->links(); e != 0; e = e->next_) {
                e->unmark();
                placeEdge(e, n);
                Node *dst = e->neighbor();
                // Should place reverse edges too
                Edge *p = dst->find_edge(n->num());
                if (p != NULL) {
                        p->unmark();
                        placeEdge(p, dst);
                }

        }
        for (Agent *a = n->agents(); a != NULL; a = a->next()) {
                a->mark(0), a->angle(NO_ANGLE);
                placeAgent(a, n);
        }
}

// we need to place edge using the edge's real angle, instead of the
// given one
void WirelessNetModel::placeEdge(Edge* e, Node* src) const
{
        if (e->marked() == 0) {
                double hyp, dx, dy;
                Node *dst = e->neighbor();
                dx=dst->x()-src->x();
                dy=dst->y()-src->y();
                hyp=sqrt(dx*dx + dy*dy);
           //   e->setAngle(atan2(dy,dx));
                double x0 = src->x() + src->size() * (dx/hyp) * .75;
                double y0 = src->y() + src->size() * (dy/hyp) * .75;
                double x1 = dst->x() - dst->size() * (dx/hyp) * .75;
                double y1 = dst->y() - dst->size() * (dy/hyp) * .75;
                e->place(x0, y0, x1, y1);

                /* Place the queue here too.  */
                EdgeHashNode *h = lookupEdgeHashNode(e->src(), e->dst());
		if (h != 0) {
                    if (h->queue != 0)
                        h->queue->place(e->size(), e->x0(), e->y0());
		}

                e->mark();
        }
}



//----------------------------------------------------------------------
// void 
// WirelessNetModel::handle(const TraceEvent& e, double now, int direction)
//   - packet handling stuff. use new packet
//   - Given a trace event parses the event type and adds wireless
//     extensions
//----------------------------------------------------------------------
void 
WirelessNetModel::handle(const TraceEvent& e, double now, int direction) {
  EdgeHashNode *ehn;
  Edge* newEdge ;
  Node *src, *dst, * n;
  View * v;

  switch (e.tt) {
    case 'a': // Agent
      NetModel::handle(e, now, direction);
      // recalculate bounding box so that all agents will 
      // be within view
      for (v = views_; v != 0; v = v->next_) {
        v->redrawModel();
      }
      break;

    case 'n': // Node
      NetModel::handle(e, now, direction);
      // no need to reposition node if the node event
      // is only for color change or label change
      if ((strncmp(e.ne.node.state, "COLOR", 5) == 0) ||
          (strncmp(e.ne.node.state, "DLABEL", 6) == 0)) {
        return;
      }

      // placing the node position
      n = lookupNode(e.ne.src);
      if (n == 0)
        return;

      n->place(e.ne.x,e.ne.y);
      n->placeorig(e.ne.x,e.ne.y);
      n->setstart(now);
      n->setend(e.ne.stoptime+now);
      n->setvelocity(e.ne.x_vel_, e.ne.y_vel_);

      moveNode(n);
      break;
  
    case 'd':
    case 'r':
      src = lookupNode(e.pe.src);
      if  (!src) {
        fprintf(stderr,"node %d is not defined... ", e.pe.src);
        break;
      }

      if (!(dst = lookupNode(e.pe.dst))) {
        if (e.pe.dst != -1) { // broadcasting address
          fprintf(stderr,"node %d is not defined... ", e.pe.dst);
          break;
        }
      }

      // Set node color based upon energy levels
      /*
      switch (e.pe.pkt.energy) {
        case 3:
          src->color("green");
          break; 
        case 2:
          src->color("yellow");
          break; 
        case 1:
          src->color("red");
          break;
        case 0:
        src->color("black");
        break;
      } 
      */

      NetModel::handle(e, now, direction);
      break;

    case 'h':
    case '+':
    case '_':
      if ( !(src= lookupNode(e.pe.src)) ) {
        fprintf(stderr, "node %d is not defined... ", e.pe.src);
        break;
      } else { 
        src->size(src->nsize());
      }

      if ( !(dst= lookupNode(e.pe.dst)) ) {
        if (e.pe.dst != -1) { //broadcasting address
          fprintf(stderr,"node %d is not defined... ", e.pe.dst);
          break;
        } else {
          /*
          if (e.pe.pkt.energy == 3) src->color("green");
          if (e.pe.pkt.energy == 2) src->color("yellow");
          if (e.pe.pkt.energy == 1) src->color("red");
          if (e.pe.pkt.energy == 0) src->color("black");
          */
          NetModel::handle(e, now, direction);
          break; 
        }
      } else {
        dst->size(dst->nsize());
      }

      /*
      if (e.pe.pkt.energy == 3) src->color("green");
      if (e.pe.pkt.energy == 2) src->color("yellow");
      if (e.pe.pkt.energy == 1) src->color("red");
      if (e.pe.pkt.energy == 0) src->color("black");
      */

      // When a wireless packet is transferred then
      // lookup to see if an edge has been created for that destination
      ehn = lookupEdgeHashNode(e.pe.src, e.pe.dst);
      if (ehn == 0 ) {
        // if not then create one dynamically
        //double bw = 1000000;
        //double delay = 1.0000000000000008e-02;
        double bw = 10.0;
        double delay = 10.0;
        double length = 0;
	// why it is 3? 
        double dsize = 3;
        double angle = 8.275783586691418e-313;
        newEdge = new Edge(src, dst, dsize, bw, delay, length, angle, 1);

        newEdge->init_color("black");
        newEdge->visible(0);
        enterEdge(newEdge);
        newEdge->Animation::insert(&drawables_);
        src->add_link(newEdge);
      }

      NetModel::handle(e, now, direction);
      break;  

    case 'l':
      if ((strncmp(e.le.link.state, "in", 2) == 0) || 
          (strncmp(e.le.link.state, "out", 3) == 0)) {
        
         // wireless process: dynamic link
        if (strncmp(e.le.link.state, "in", 2)==0) {
          if (direction==FORWARDS) {
            Tcl& tcl = Tcl::instance();
            tcl.evalf("%s add_link {%s}", name(), e.image);
          } else {
            // remove the link
            removeLink(e);
          } 
        } 

        if (strncmp(e.le.link.state, "out", 3)==0) {
          if (direction==FORWARDS) {
            removeLink(e);
          } else {
            Tcl& tcl = Tcl::instance();
            tcl.evalf("%s add_link {%s}", name(), e.image);
          }
        }
      } else {
        NetModel::handle(e, now, direction);
      }      
      break;

    default:
      NetModel::handle(e, now, direction);
      break;
  }
}

void WirelessNetModel::addToEdgePool(Edge* e)
{
    WEdgeList* p = new WEdgeList;
    p->wEdge = e;
    p->next = edges_ ;
    edges_ = p;
}

Edge* WirelessNetModel::removeFromEdgePool()
{
    if (edges_) {
        WEdgeList* p = edges_ ;
        edges_ = p->next;
	Edge* w = p->wEdge ;
        delete p ;
	return w ;
    }
    else return (Edge*) 0 ;
}


//----------------------------------------------------------------------
// void 
// WirelessNetModel::removeLink(const TraceEvent& e)
//----------------------------------------------------------------------
void WirelessNetModel::removeLink(const TraceEvent& e) {
  int src = e.le.src;
  int dst = e.le.dst;

  EdgeHashNode * h = lookupEdgeHashNode(src, dst);
  EdgeHashNode * g = lookupEdgeHashNode(dst, src);
  if (h == 0 || g == 0) {
    // h,g = 0 or h,g !=0
    return;
  }
                
  Edge * e1 = h->edge; 
  Edge * e2 = g->edge;
           
  removeEdge(e1);
  removeEdge(e2);

  e1->detach();
  e2->detach();
 
  Node* srcnode = e1->start();
  Node* dstnode = e2->start();

  // it is a duplex by default
  srcnode->delete_link(e1);
  dstnode->delete_link(e2);
                        
  delete e1;
  delete e2;
}

void WirelessNetModel::removeLink(int src, int dst)
{
                    EdgeHashNode *h = lookupEdgeHashNode(src, dst);
                    if (h == 0 ) return;
               
                    Edge* e1 = h->edge; 
		    if (e1 == 0) return ;
           
		    e1->dec_usage();
		    if (e1->used() != 0) return;

                    removeEdge(e1);

                    e1->detach();
 
                    Node* srcnode = e1->start();

                    srcnode->delete_link(e1);
                        
                    delete e1;

}

