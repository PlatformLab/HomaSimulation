/*
 * Copyright (c) 1991,1993-1994 Regents of the University of California.
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
 *    This product includes software developed by the Computer Systems
 *    Engineering Group at Lawrence Berkeley Laboratory.
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
 * @(#) $Header: /cvsroot/nsnam/nam-1/netmodel.h,v 1.43 2003/01/28 03:22:38 buchheim Exp $ (LBL)
 */

#ifndef nam_netmodel_h
#define nam_netmodel_h

#define NO_ANGLE 10

// The ratio of node radius and mean edge length in the topology
const float NODE_EDGE_RATIO = 0.15; 
const int WIRELESS_SCALE = 66666;

class Edge;
class Node;
class Lan;
class Route;
class Agent;
struct TraceEvent;
class Queue;
class Animation;
class View;
class NetView;
class PSView;
class TestView;
class Paint;
class Packet;
class Group;
class Tag;
struct BBox;
class EditView;

#include "trace.h"
#include "monitor.h"

/*
 * The underlying model for a NetView.  We factor this state out of the
 * NetView since we might have multiple displays of the same network
 * (i.e., zoomed windows), as opposed to multiple interpretations 
 * (i.e, strip charts).
 */
class NetModel : public TraceHandler {
public:
  NetModel(const char *animator);
  virtual ~NetModel();

  void render(View*);
  void render(PSView*);
  void render(TestView*);
  void update(double, Animation*);

  // Only redraw the part within a rectangle. 
  // Follows Tk_CanvasEventuallyRedraw
  virtual void render(EditView*, BBox &bb);

  /* TraceHandler hooks */
  virtual void update(double);
  void reset(double);
  void handle(const TraceEvent&, double now, int direction);

  virtual void BoundingBox(BBox&);
  void addView(NetView*);
  //Animation* inside(double now, float px, float py) const;
  Animation* inside(float px, float py) const;

  int add_monitor(Animation *a);
  void delete_monitor(Monitor *m);
  void delete_monitor(Animation *a);
  void delete_monitor(int monnum);
  void check_monitors(Animation *a);
  int monitor(double now, int monitor, char *result, int len);

  Packet *newPacket(PacketAttr &pkt, Edge *e, double time);
  void color_subtrees();
  void set_wireless();
  void selectPkt(int , int , int );
  
  int save_layout(const char *filename);

  // Tagging methods
  int tagCmd(View *v, int argc, char **argv,
  char *newTag, char *cmdName);
  Animation* findClosest(float dx, float dy, double halo);
  void tagObject(Tag *tag, Animation *);
  int tagArea(BBox &bb, Tag *tag, int bEnclosed);
  int deleteTagCmd(char *tagName, char *tagDel);

  int add_tag(Tag *tag);
  void delete_tag(const char *tn);
  Tag* lookupTag(const char *tn);

  inline double now() { return now_; }

  // If editing is enabled, these functions must be public
  virtual void moveNode(Node *n);
  virtual void recalc() {}

  void remove_view(View *v);

  void add_drop(const TraceEvent &, double now, int direction);

  // Create Nodes and Edges(One Way Links) and insert them into this 
  // NetModel's list of drawables
//  Node * addNode(int argc, const char *const *argv);
  Node * addNode(const TraceEvent &e);

  Edge * addEdge(int argc, const char *const *argv);
  Edge * addEdge(int src_id, int dst_id, const TraceEvent &e);

  Node* lookupNode(int nn) const;
  Edge * lookupEdge(int source, int destination) const;
  
  virtual void placeAgent(Agent* a, Node* src) const;
  void hideAgentLinks();
  virtual void relayout(){};

protected:
  virtual void scale_estimate();
  int traverseNodeConnections(Node* n);
  void move(double& x, double& y, double angle, double d) const;
  virtual void placeEverything();
  void placeEdgeByAngle(Edge* e, Node* src) const;
  virtual void placeEdge(Edge* e, Node* src) const;

  int addr2id(int addr) const;
  int addAddress(int id, int addr) const;
  void removeNode(Node *n);
  Agent* lookupAgent(int id) const;
  Lan* lookupLan(int nn) const;
  Packet *lookupPacket(int src, int dst, int id) const;

  int command(int argc, const char*const* argv);

#define EDGE_HASH_SIZE 256
  inline int ehash(int src, int dst) const {
    return (src ^ (dst << 4)) & (EDGE_HASH_SIZE-1);
  }

  struct EdgeHashNode {
    EdgeHashNode* next;
    int src;
    int dst;
    Edge* edge;
    Queue* queue;
  };

  Tcl_HashTable *addrHash_;

  EdgeHashNode * lookupEdgeHashNode(int source, int destination) const;
  void enterEdge(Edge* e);
  void removeEdge(Edge* e);
  void saveState(double);

  Animation *drawables_;  // List of objects to draw
  Animation *animations_; // List of objects to draw
  Queue *queues_;
  View* views_;
  Node* nodes_;
  Lan* lans_;
  double now_;
  double wirelessNodeSize_;
  double packet_size_;  // Initially set to 2.5
                        // Which is half of the default node size of 10.0
                        // default node size is defined in parser.cc
                        
  double nymin_, nymax_, node_size_, node_sizefac_; /*XXX*/
  int mon_count_;
  Monitor *monitors_;
  EdgeHashNode* hashtab_[EDGE_HASH_SIZE];

  int nclass_;    // no. of classes (colors)
  int wireless_ ;         // in a wireless network ?
  int resetf_ ;
  int* paint_;
  int* oldpaint_;
  int paintMask_;    // Mask to convert flow id to color id

  //selected filter value
  char selectedTraffic_[PTYPELEN] ;
  int selectedSrc_ ;
  int selectedDst_ ;
  int selectedFid_ ;
  char hideTraffic_[PTYPELEN] ;
  int hideSrc_ ;
  int hideDst_ ;
  int hideFid_ ;
  char colorTraffic_[PTYPELEN] ;
  int colorSrc_ ;
  int colorDst_ ;
  int colorFid_ ;
  int showData_ ;
  int showRouting_ ;
  int showMac_ ;
  int selectedColor_ ;

  // Map group address to group id
  Tcl_HashTable *grpHash_;
  int nGroup_;
  int add_group(Group *grp);
  Group* lookupGroup(unsigned int addr);

  // Map tag name (string) to tag id
  Tcl_HashTable *tagHash_;
  int nTag_;

  // Map object class name to class id
  Tcl_HashTable *objnameHash_;
  int registerObjName(const char*, int);
  int lookupObjname(const char *);
  Animation* lookupTagOrID(const char *);

  virtual void relayoutNode(Node *n) {}

private:
  TraceEvent traceevent_;  // Used for parsing node creation events which   
  ParseTable parsetable_;  // come from netModel.tcl.  I wanted to use
                             // the parsetable from Trace.h but cannot 
                             // figure out how to access it. Data from
                             // parsed lines is put into trace_event_.

  double bcast_duration_;  // how long broadcast packets are visible
  double bcast_radius_;    // radius of broadcast packets

};

#endif
