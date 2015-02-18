/*
 * Copyright (C) 1997 by the University of Southern California
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * The copyright of this module includes the following
 * linking-with-specific-other-licenses addition:
 *
 * In addition, as a special exception, the copyright holders of
 * this module give you permission to combine (via static or
 * dynamic linking) this module with free software programs or
 * libraries that are released under the GNU LGPL and with code
 * included in the standard release of ns-2 under the Apache 2.0
 * license or under otherwise-compatible licenses with advertising
 * requirements (or modified versions of such code, with unchanged
 * license).  You may copy and distribute such a system following the
 * terms of the GNU GPL for this module and the licenses of the
 * other code concerned, provided that you include the source code of
 * that other code when and as the GNU GPL requires distribution of
 * source code.
 *
 * Note that people who make modified versions of this module
 * are not obligated to grant this special exception for their
 * modified versions; it is their choice whether to do so.  The GNU
 * General Public License gives permission to release a modified
 * version without this exception; this exception also makes it
 * possible to release a modified version which carries forward this
 * exception.
 *
 * $Header: /cvsroot/nsnam/nam-1/editview.cc,v 1.36 2007/02/12 07:08:43 tom_henderson Exp $
 */

#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#endif

#include <ctype.h>
#include <math.h>

#include "view.h"
#include "bbox.h"
#include "netmodel.h"
#include "editview.h"
#include "tclcl.h"
#include "paint.h"
#include "tag.h"
#include "node.h"
#include "edge.h"
#include "agent.h"

void EditView::DeleteCmdProc(ClientData cd)
{
	EditView *ev = (EditView *)cd;
	if (ev->tk_ != NULL) {
		Tk_DestroyWindow(ev->tk_);
	}
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
EditView::EditView(const char *name, NetModel* m) 
        : NetView(name), defTag_(NULL)
{
	if (tk_!=0) {
		Tcl& tcl = Tcl::instance();
		cmd_ = Tcl_CreateCommand(tcl.interp(), Tk_PathName(tk_), 
					 command, 
					 (ClientData)this, DeleteCmdProc);
	}
	char str[256];
	model_ = m;
	sprintf(str, "def%-u", (long)this);
	defTag_ = new Tag(str);
	model_->add_tag(defTag_);
	editing_stage_ = NONE;
}

//----------------------------------------------------------------------
// EditView::EditView(const char *name, NetModel* m,
//                    int width, int height)
//----------------------------------------------------------------------
EditView::EditView(const char *name, NetModel* m, int width, int height)
  : NetView(name, SQUARE, width, height), defTag_(NULL) {

	char str[256];

	if (tk_!=0) {
		Tcl& tcl = Tcl::instance();
		cmd_ = Tcl_CreateCommand(tcl.interp(), Tk_PathName(tk_),
		                         command, (ClientData) this,
		                         DeleteCmdProc);
	}

	model_ = m;
	sprintf(str, "def%-u", (long)this);
	defTag_ = new Tag(name);
	model_->add_tag(defTag_);
	editing_stage_ = NONE;
}

EditView::~EditView()
{
	model_->remove_view(this);
	if (defTag_ != NULL) {
		model_->delete_tag(defTag_->name());
		delete defTag_;
		defTag_ = NULL;
	}
	// Delete Tcl command created
//  	Tcl& tcl = Tcl::instance();
//  	Tcl_DeleteCommandFromToken(tcl.interp(), cmd_);
}

//----------------------------------------------------------------------
// int 
// EditView::command(ClientData cd, Tcl_Interp* tcl,
//                   int argc, char **argv)
//    Parse Tcl command interface
//      - setPoint, addNode, addLink addAgent, deleteObject, etc...
//----------------------------------------------------------------------
int EditView::command(ClientData cd, Tcl_Interp* tcl,
                      int argc, CONST84 char **argv) {
  EditorNetModel * editor_net_model;
  Node * node, * source, * destination;
  Edge * edge;
  Agent * source_agent, * destination_agent;
  TrafficSource * traffic_source;
  LossModel * loss_model;
  //double world_x, world_y;
  
  if (argc < 2) {
    Tcl_AppendResult(tcl, "\"", argv[0], "\": arg mismatch", 0);
    return (TCL_ERROR);
  }

  char c = argv[1][0];
  int length = strlen(argv[1]);
  EditView * ev = (EditView *) cd;
  editor_net_model = (EditorNetModel *) ev->model_;

  float cx, cy;
  int flag;

  // Following implements several useful interactive commands of 
  // Tk's canvas. Their syntax and semantics are kept as close to
  // those of Tk canvas as possible.

  if ((c == 'd') && (strncmp(argv[1], "draw", length) == 0)) {
    // Redraw seen at time = argv[2]
    ev->draw(strtod(argv[2],NULL));
    return TCL_OK;

  } else if ((c == 'c') && (strncmp(argv[1], "close", length) == 0)) {
    ev->destroy();
    return TCL_OK;

  } else if ((c == 'd') &&
             (strncmp(argv[1], "dctag", length) == 0)) {
    /*
     * <view> dctag
     * 
     * Delete the current selection, start all over.
     */
    if (ev->defTag_ != NULL)
      ev->defTag_->remove();
    ev->draw();
    return (TCL_OK);

  } else if ((c == 's') &&
             (strncmp(argv[1], "setPoint", length) == 0)) {
    /*
     * <view> setPoint x y addFlag
     * 
     * Select the object (if any) under point (x, y). If no such
     * object, set current point to (x, y)
     */
    if (argc != 5) {
      Tcl_AppendResult(tcl, "wrong # args: should be \"",
                            argv[0], " setPoint x y addFlag\"", 
                            (char *)NULL);
      return (TCL_ERROR);
    }
    cx = strtod(argv[2], NULL);
    cy = strtod(argv[3], NULL);
    flag = strtol(argv[4], NULL, 10);
    return ev->cmdSetPoint(cx, cy, flag);

  } else if ((c == 'd') && (strncmp(argv[1], "deleteObject", length) == 0)) { 
    cx = strtod(argv[2], NULL);
    cy = strtod(argv[3], NULL);
    return  ev->cmdDeleteObj(cx, cy);

  } else if ((c == 'm') && (strncmp(argv[1], "moveTo", length) == 0)) { 
    /*
     * <view> moveto x y
     * 
     * Move the current selected object (if any) or the 
     * rubber band to point (x, y)
     */
    if (argc != 4) {
      Tcl_AppendResult(tcl, "wrong # args: should be \"",
                             argv[0],
                            " moveTo x y\"", (char *)NULL);
      return (TCL_ERROR);
    }
    cx = strtod(argv[2], NULL);
    cy = strtod(argv[3], NULL);
    return ev->cmdMoveTo(cx, cy);

  } else if ((c == 'r') && (strncmp(argv[1], "releasePoint", length) == 0)) {
    /*
     * <view> releasePoint x y
     * 
     * If any object is selected, set the object's
     * position to point (x,y); otherwise set the rubber
     * band rectangle and select all the objects in that
     * rectangle Note: we need a default tag for the
     * selection in rubber band.  
     */
    if (argc != 4) {
      Tcl_AppendResult(tcl, "wrong # args: should be \"",
           argv[0],
           " releasePoint x y\"", (char *)NULL);
      return (TCL_ERROR);
    }
    cx = strtod(argv[2], NULL);
    cy = strtod(argv[3], NULL);
    return ev->cmdReleasePoint(cx, cy);


  } else if ((c == 's') && (strncmp(argv[1], "setNodeProperty", length) == 0)) {
    // <view> setNodeProperty nodeid nodepropertyvalue properyname
    int nodeid = atoi(argv[2]);
    return ev->cmdsetNodeProperty(nodeid, argv[3], argv[4]);

  } else if ((c == 's') && (strncmp(argv[1], "setAgentProperty", length) == 0)) {
     // <view> setAgentProperty nodeid nodepropertyvalue properyname
    int agentid = atoi(argv[2]);
    return ev->cmdsetAgentProperty(agentid, argv[3], argv[4]);

  } else if ((c == 's') && (strncmp(argv[1], "setLinkProperty", length) == 0)) {
    // <view> setLinkProperty sid did propertyvalue properyname
    int source_id = atoi(argv[2]);
    int destination_id = atoi(argv[3]);
    return ev->cmdsetLinkProperty(source_id, destination_id,
                                  argv[4], argv[5]);
  } else if ((c == 's') &&
             (strncmp(argv[1], "setQueueHandleProperty", length) == 0)) {
    // Called by the following code in Editor.tcl
    // ------------------------------------------------------------
    // $editor_view_ setQueueHandleProperty $source $destination 
    //                                      $property_value 
    //                                      $property_variable
    //
    //  - argv[2] = source
    //  - argv[3] = destination
    //  - argv[4] = value
    //  - argv[5] = variable name

    editor_net_model->setQueueHandleProperty(atoi(argv[2]), atoi(argv[3]),
                                              argv[4], argv[5]);

    ev->draw();
    return TCL_OK;

  } else if ((c == 's') &&
             (strncmp(argv[1], "setTrafficSourceProperty", length) == 0)) {
    // <view> setTrafficSourceProperty id propertyvalue propertyname
    int id = atoi(argv[2]);
    const char * value = argv[3];
    const char * variable_name = argv[4];
    return ev->cmdsetTrafficSourceProperty(id, value, variable_name);
 
  } else if ((c == 's') &&
             (strncmp(argv[1], "setLossModelProperty", length) == 0)) {
    // <view> setLossModelProperty id propertyvalue propertyname
    int id = atoi(argv[2]);
    const char * value = argv[3];
    const char * variable_name = argv[4];
    return ev->cmdsetLossModelProperty(id, value, variable_name);

  } else if ((c == 'a') && (strncmp(argv[1], "addNode", length) == 0)) {
    // <view> addNode x y
    //   - add a new Node under current (x,y) with default size
    cx = strtod(argv[2], NULL);
    cy = strtod(argv[3], NULL);
    return ev->cmdaddNode(cx, cy);

  } else if ((c == 'i') && (strncmp(argv[1], "importNode", length) == 0)) {
    // Used when reading a nam editor file to add a node created in tcl
    // to this editview and its editornetmodel

    node = (Node *) TclObject::lookup(argv[2]);
    //world_x = atof(argv[3]);
    //world_y = atof(argv[4]);

    if (node) {
      if (editor_net_model->addNode(node)) {
     //   node->place(world_x, world_y);
     //   ev->draw();
        return TCL_OK;
      }
    }

    fprintf(stderr, "Error creating node.\n");
    return TCL_ERROR;

  } else if ((c == 'a') && (strncmp(argv[1], "attachNodes", length) == 0)) {
    // <view> linkNodes source_id destination_id
    //   - link 2 existing nodes to each other

    edge = (Edge *) TclObject::lookup(argv[2]);
    source = (Node *) TclObject::lookup(argv[3]);
    destination = (Node *) TclObject::lookup(argv[4]);
    edge->attachNodes(source, destination);
    editor_net_model->addEdge(edge);

    ev->draw();  
    return TCL_OK;

  } else if ((c == 'a') && (strncmp(argv[1], "addLink", length) == 0)) {
    //  <view> addLink x y 
    //    - add a new Link if its start AND end point is inside of 
    cx = strtod(argv[2], NULL);
    cy = strtod(argv[3], NULL);
    return ev->cmdaddLink(cx, cy);

  } else if ((c == 'a') && (strncmp(argv[1], "addQueueHandle", length) == 0)) {
    //  <view> addQueueHandle edge queue_type
    edge = (Edge *) TclObject::lookup(argv[2]);
    if (edge) {
      editor_net_model->addQueueHandle(edge, argv[3]);
    }
    return TCL_OK;
    

  
  // --------------- Agent -----------------
  } else if ((c == 'a') && (strncmp(argv[1], "addAgent", length) == 0)) {
    // <view> addAgent x y agent
    //   - add a new agent
    cx = strtod(argv[2], NULL);
    cy = strtod(argv[3], NULL);
    const char * agent_name = argv[4];
    return ev->cmdaddAgent(cx, cy, agent_name);

  } else if ((c == 'i') && (strncmp(argv[1], "importAgent", length) == 0)) {
    source_agent = (Agent *) TclObject::lookup(argv[2]);
    node = (Node *) TclObject::lookup(argv[3]);
    return editor_net_model->addAgent(source_agent, node);

  } else if ((c == 'l') && (strncmp(argv[1], "linkAgents", length) == 0)) {
    // <view> linkAgents source_id destination_id
    //   - link 2 existing agents to each other
    source_agent = (Agent *) TclObject::lookup(argv[2]);
    destination_agent = (Agent *) TclObject::lookup(argv[3]);
    return editor_net_model->addAgentLink(source_agent, destination_agent);

  } else if ((c == 's') && (strncmp(argv[1], "showAgentLink", length) == 0)) {
    cx = strtod(argv[2], NULL);
    cy = strtod(argv[3], NULL);
    return ev->showAgentLink(cx, cy);

  } else if ((c == 'h') && (strncmp(argv[1], "hideAgentLinks", length) == 0)) {
    editor_net_model->hideAgentLinks();
    ev->draw();
    return TCL_OK;
	 
  // --------------- Traffic Source -----------------
  } else if ((c == 'a') &&
             (strncmp(argv[1], "addTrafficSource", length) == 0)) {
    // <view> addTrafficSource x y trafficsource_type
    //   -  add a new TrafficSource
    cx = strtod(argv[2], NULL);
    cy = strtod(argv[3], NULL);
    // argv[4] is the name of the traffic source
    return ev->cmdaddTrafficSource(cx, cy, argv[4]);

  } else if ((c == 'a') &&
             (strncmp(argv[1], "attachTrafficSource", length) == 0)) {
    traffic_source = (TrafficSource *) TclObject::lookup(argv[2]);
    source_agent = (Agent *) TclObject::lookup(argv[3]);
    return editor_net_model->addTrafficSource(traffic_source, source_agent);


  // ----------- Loss Models ----------------
  } else if ((c == 'a') &&
             (strncmp(argv[1], "addLossModel", length) == 0)) {
    // <view> addLossModel x y lossmodel
    cx = strtod(argv[2], NULL);
    cy = strtod(argv[3], NULL);
    // argv[4] is the type of the loss model to add
    return ev->cmdaddLossModel(cx, cy, argv[4]);

  } else if ((c == 'a') &&
             (strncmp(argv[1], "attachLossModel", length) == 0)) {
    loss_model = (LossModel *) TclObject::lookup(argv[2]);
    source = (Node *) TclObject::lookup(argv[3]);
    destination = (Node *) TclObject::lookup(argv[4]);

    edge = source->find_edge(destination->number());
    return editor_net_model->addLossModel(loss_model, edge);

  
  // ----------- Object and Object Properties ----------------
  } else if ((c == 'g') &&
             (strncmp(argv[1], "getObjectInformation", length) == 0)) {
 
    // <view> getObject x y
    //   - return object under current point

    cx = strtod(argv[2], NULL);
    cy = strtod(argv[3], NULL);

    return ev->cmdgetObjectInformation(cx, cy);

  } else if ((c == 'g') &&
             (strncmp(argv[1], "getObjectProperty", length) == 0)) {
    // <view> getObjectPropert x y [type]
    //  - return properties for object under current point
    //  - if type is passed in then pass that to object

    cx = strtod(argv[2], NULL);
    cy = strtod(argv[3], NULL);

    if (argc == 5) {
      return ev->cmdgetObjectProperties(cx, cy, argv[4]);
    } else {
      return ev->cmdgetObjProperty(cx, cy);
    }


  } else if ((c == 'v') && (strncmp(argv[1], "view_mode", length)==0)) {
    /*
     * <view> view_mode
     * 
     * clear defTag_ and change to view mode
     */
    ev->view_mode();
    return TCL_OK;
  }

  return (NetView::command(cd, tcl, argc, argv));
}

int EditView::cmdsetNodeProperty(int id, const char * value, const char * variable)
{
	EditorNetModel* emodel_ = (EditorNetModel *) model_;	
  // goto enetmodel.cc
	emodel_->setNodeProperty(id, value, variable);

//	EditType oldt_;
//	oldt_ = editing_stage_;
//	editing_stage_ = END_OBJECT;
	draw();
//	editing_stage_ = NONE;
//	model_->update(model_->now());
//	editing_stage_ = oldt_;

	return(TCL_OK);
}

int EditView::cmdsetAgentProperty(int id, const char * value, const char * variable) {
	EditorNetModel* emodel_ = (EditorNetModel *) model_;
	emodel_->setAgentProperty(id, value ,variable);
	draw();  
	return(TCL_OK);
}


int EditView::cmdsetLinkProperty(int sid, int did,
                                 const char * value, const char * variable) {
  EditorNetModel * emodel_ = (EditorNetModel *) model_;
  emodel_->setLinkProperty(sid, did, value, variable);
  draw();
  return(TCL_OK);
}

//----------------------------------------------------------------------
// int
// EditView::cmdsetTrafficSourceProperty(int sid, int did, const char *pv, int pn)
//----------------------------------------------------------------------
int
EditView::cmdsetTrafficSourceProperty(int id, const char * value, const char * variable) {
  EditorNetModel* emodel_ = (EditorNetModel *) model_;
  emodel_->setTrafficSourceProperty(id, value, variable);
  draw();
  return(TCL_OK);
}        

//----------------------------------------------------------------------
// int
// EditView::cmdsetLossModelProperty(int id, char * value, char * variable) 
//----------------------------------------------------------------------
int
EditView::cmdsetLossModelProperty(int id, const char * value, const char * variable) {
  EditorNetModel* emodel_ = (EditorNetModel *) model_;
  emodel_->setLossModelProperty(id, value, variable);
  draw();
  return(TCL_OK);
}        

//----------------------------------------------------------------------
// int 
// EditView::cmdgetObjProperty(float cx, float cy)
//   - get properties for a selected object
//----------------------------------------------------------------------
int 
EditView::cmdgetObjProperty(float cx, float cy) { 
	Animation *p;
	Tcl& tcl = Tcl::instance();

	// matrix_ comes from class View (view.h)
	//  - imap is the inverse mapping
	//    (i.e from screen to world coordinate systems)
	matrix_.imap(cx, cy);

	// Finds the object which contains the coordinate (cx, cy)
	p = model_->inside(cx, cy);
	
	if (p == NULL) {
	   tcl.resultf("NONE");
	} else {
	   tcl.resultf("%s",p->property());
 	}
	return(TCL_OK);	
}


//----------------------------------------------------------------------
// int 
// EditView::cmdgetObjectProperties(float cx, float cy, char * type)
//   - get properties for a selected object and the specific 
//     type within that object
//----------------------------------------------------------------------
int 
EditView::cmdgetObjectProperties(float cx, float cy, const char * type) { 
	Animation * animation_object;
	Tcl& tcl = Tcl::instance();

	// matrix_ comes from class View (view)
	//  - imap is the inverse mapping
	//    (i.e from screen to world coordinate systems)
	matrix_.imap(cx, cy);

	// Finds the object which contains the coordinate (cx, cy)
	animation_object = model_->inside(cx, cy);
	
	if (animation_object == NULL) {
	   tcl.resultf("NONE");
	} else {
	   tcl.resultf("%s",animation_object->getProperties(type));
	}
	return(TCL_OK);	
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
int EditView::cmdgetObjectInformation(float cx, float cy) { 
	Tcl& tcl = Tcl::instance();
	matrix_.imap(cx, cy);
	Animation *p = model_->inside(cx, cy);

	if (p == NULL) {
		tcl.resultf("NONE");
	} else {
		tcl.resultf("%s",p->info());
	}
	return(TCL_OK);	
}


//----------------------------------------------------------------------
// int
// EditView::cmdaddLink(float cx, float cy)
//   - Note: all cx_ and cy_ below are in *window* coordinates
//----------------------------------------------------------------------
int
EditView::cmdaddLink(float cx, float cy) {
  Animation * p;
  static Animation * old_p;
  EditorNetModel* emodel_ = (EditorNetModel *) model_;

  cx_ = cx;
  cy_ = cy;
  matrix_.imap(cx, cy);

  // Do we have a node  on current point ?
  p = model_->inside(cx, cy);

  if (p == NULL) {
    defTag_->remove();
    if (editing_stage_ == START_LINK) {
       editing_stage_ = NONE;
      draw();
    }
    return (TCL_OK);
  }

  if (p->classid() == ClassNodeID) {

    if (editing_stage_ != START_LINK) {
      old_p = p;                /* remember the orig node */
      startSetObject(p, cx_, cy_);
      editing_stage_ = START_LINK;

     } else if ((old_p == p)||(old_p->classid()!=ClassNodeID)) {
       editing_stage_ = NONE;
       draw();
       // to support making link by click-&-click 
       editing_stage_ = START_LINK; 

     } else {
       // add a new link b/w the two nodes
       startSetObject(p, cx_, cy_);

       Node *src, *dst;
       src = (Node *)old_p;
       dst = (Node *)p;
       emodel_->addLink(src, dst);
       editing_stage_ = END_OBJECT;
     
       // Erase old positions
       defTag_->draw(this, model_->now());
       // At least we should redo scale estimation and
       // place everything
       defTag_->move(this, rb_.xmax - oldx_, rb_.ymax - oldy_);
       model_->recalc();
       model_->render(this);
       defTag_->remove();
       editing_stage_ = NONE;
       draw();
       editing_stage_ = NONE;
     }

  } else if (p->classid() == ClassAgentID) {
    if (editing_stage_ != START_LINK) {
      old_p = p;
      startSetObject(p, cx_, cy_);
      editing_stage_ = START_LINK;
    } else if ((old_p == p)||(old_p->classid()!= ClassAgentID)) {
      editing_stage_ = NONE;
      draw();
      // to support making agent-link by click-&-click
      editing_stage_ = START_LINK;
    } else {
      startSetObject(p, cx_, cy_);

      Agent *src_agent, *dst_agent;
      src_agent = (Agent *)old_p;
      dst_agent = (Agent *)p;
      emodel_->addAgentLink(src_agent, dst_agent);
      editing_stage_ = END_OBJECT;

      defTag_->draw(this, model_->now());
      defTag_->move(this, rb_.xmax - oldx_, rb_.ymax - oldy_);
      model_->recalc();
      model_->render(this);
      defTag_->remove();
      editing_stage_ = NONE;
      draw();
    }
  } else {
    editing_stage_ = NONE;
  }
  return(TCL_OK);
}


//----------------------------------------------------------------------
// int 
// EditView::cmdaddAgent(float cx, float cy, char* agent)
//----------------------------------------------------------------------
int EditView::cmdaddAgent(float cx, float cy, const char* agent) {
  Animation * p;
  Node * src;
  EditorNetModel* emodel_ = (EditorNetModel *)model_;

  // Switch from screen coordinates to world coordinates
  matrix_.imap(cx, cy);

  // Do we have a node on the clicked point ?
  p = model_->inside(cx, cy);
  if (p == NULL) {
    defTag_->remove();
    return (TCL_OK);
  } 

  if (p->classid() == ClassNodeID) {
    // Yes, we have a node to which we can add the agent
    src = (Node *) p;
    // goto EditorNetModel::addAgent(...) in enetmodel.cc
    emodel_->addAgent(src, agent, cx, cy);
    draw();
  } else {
    return (TCL_OK);
  }
  return(TCL_OK);
}

//----------------------------------------------------------------------
// int EditView::showAgentLink(float cx, float cy)
//----------------------------------------------------------------------
int EditView::showAgentLink(float cx, float cy) {
	Animation * animation;
	Agent * agent;

	// Change from screen coordinates to worl coordinates
	matrix_.imap(cx, cy);

	// Get the clicked on animation object.
	animation = model_->inside(cx, cy);

	if (animation != NULL && animation->classid() == ClassAgentID) {
		agent = (Agent *) animation;
		agent->showLink();
		draw();
	}

	return(TCL_OK);	
}

//----------------------------------------------------------------------
// int 
// EditView::cmdaddNode(float cx, float cy)
//   Note: all cx_ and cy_ below are in *window* coordinates
//----------------------------------------------------------------------
int EditView::cmdaddNode(float cx, float cy) {
  Animation * animation_object;
  EditorNetModel* emodel_ = (EditorNetModel *)model_;

  // Convert screen coordinates to world coordinates
  matrix_.imap(cx, cy);

  // Check to see if an object already exists on the click point
  animation_object = model_->inside(cx,cy);
  if (animation_object == NULL) {
    defTag_->remove();
    emodel_->addNode(cx, cy);		
    draw();
  } 
  return(TCL_OK);
}

//----------------------------------------------------------------------
// int
// EditView::cmdDeleteObj(float cx, float cy)
//----------------------------------------------------------------------
int
EditView::cmdDeleteObj(float cx, float cy) {
  Animation * p;  // I don't know why p was chosen for the varible name
                  // Just to confuse other programmers, I guess
  
  // First of all, clear the old point 
  cx_ = cx;
  cy_ = cy;
                
  // Do we have an animation object on the clicked point?
  matrix_.imap(cx, cy);
  p = model_->inside(cx, cy);
  if (p == NULL) {
    defTag_->remove();
    return (TCL_OK);
  }       
  
  // If object is already selected unselect it
  if (p->isTagged()) {
    if (p->numTag() > 1) {
      fprintf(stderr, "Error: More than one tag for an object %d!\n",
                       p->id());
      p = NULL;
    } else {
      p = p->getLastTag();
    }
  }

  // Only nodes, links, agents, traffic sources and
  // loss models can be deleted
  if ((p->classid() != ClassNodeID) &&
      (p->classid() != ClassEdgeID) &&
      (p->classid() != ClassAgentID) &&
      (p->classid() != ClassTrafficSourceID) &&
      (p->classid() != ClassLossModelID)) {
    p = NULL;
  }

  if (p == NULL) {
    defTag_->remove();
  } else {
    if (p->classid() == ClassNodeID) {
      Node * n = (Node *) p;
      EditorNetModel * emodel_ = (EditorNetModel *) model_;
      emodel_->removeNode(n);
      draw();
    } 
    if (p->classid() == ClassEdgeID) {
      Edge * e = (Edge *) p;
      EditorNetModel * emodel_ = (EditorNetModel *) model_;
      emodel_->removeLink(e);
      draw();
    } 
    if (p->classid() == ClassAgentID) {
      Agent * a = (Agent *) p;
      EditorNetModel* emodel_ = (EditorNetModel *) model_;
      emodel_->removeAgent(a);
      draw();
    }
    if (p->classid() == ClassTrafficSourceID) {
      TrafficSource * ts = (TrafficSource *) p;
      EditorNetModel* emodel_ = (EditorNetModel *) model_;
      emodel_->removeTrafficSource(ts);
      draw();
    }
    if (p->classid() == ClassLossModelID) {
      LossModel * lossmodel = (LossModel *) p;
      EditorNetModel* emodel_ = (EditorNetModel *) model_;
      emodel_->removeLossModel(lossmodel);
      draw();
    }

  }

  return (TCL_OK);
}

//----------------------------------------------------------------------
// int 
// EditView::cmdaddTrafficSource(float cx, float cy, char * type)
//----------------------------------------------------------------------
int
EditView::cmdaddTrafficSource(float cx, float cy, const char * type) {
  Animation * object;
  Agent * agent;
  EditorNetModel* emodel_ = (EditorNetModel *)model_;

  // Switch from screen coordinates to world coordinates
  matrix_.imap(cx, cy);

  // Do we have an agent on the clicked point ?
  object = model_->inside(cx, cy);
  if (object == NULL) {
    defTag_->remove();
    return (TCL_OK);
  } 

  if (object->classid() == ClassAgentID) {
    // Yes, we have a node to which we can add the agent
    agent = (Agent *) object;
    // goto EditorNetModel::addTrafficSource(...) in enetmodel.cc
    emodel_->addTrafficSource(agent, type, cx, cy);
    draw();
  } else {
    return (TCL_OK);
  }
  return(TCL_OK);
}

//----------------------------------------------------------------------
// int 
// EditView::cmdaddLossModel(float cx, float cy, char * type)
//----------------------------------------------------------------------
int
EditView::cmdaddLossModel(float cx, float cy, const char * type) {
	Animation * object;
	Node * source, * destination;
	Edge * edge, * reverse_edge;
	EditorNetModel* emodel_ = (EditorNetModel *) model_;

	// Switch from screen coordinates to world coordinates
	matrix_.imap(cx, cy);

	// Do we have a link on the clicked point ?
	object = model_->inside(cx, cy);
	if (object == NULL) {
		defTag_->remove();
		return (TCL_OK);
	} 

	if (object->classid() == ClassEdgeID ||
			object->classid() == ClassQueueHandleID ) {
		//
		// Yes, we have a possible edge to which we can add the loss model

		if (object->classid() == ClassQueueHandleID ) {
			edge = ((QueueHandle *) object)->getEdge();
		} else {
			edge = (Edge *) object;
		}

		if (edge) {
			// Check distance to source to determine if we should attach 
			// to the forward or reverse edge
			source = edge->getSourceNode();
			destination = edge->getDestinationNode();

			if (source->distance(cx, cy) > destination->distance(cx,cy)) {
				// if source is farther away than destination then place
				// this loss model on the reverse edge
				reverse_edge = emodel_->lookupEdge(destination->number(), source->number());
				if (reverse_edge) { // Make sure there is a reverse edge
					edge = reverse_edge;
				}
			}

			// goto EditorNetModel::addTrafficSource(...) in enetmodel.cc
			emodel_->addLossModel(edge, type, cx, cy);
			draw();
		}
	}

	return(TCL_OK);
}

// ---------------------------------------------------------------------
// int 
// EditView::cmdSetPoint(float cx, float cy, int bAdd)
//   - Note: all cx_ and cy_ below are in *window* coordinates
// ---------------------------------------------------------------------
int 
EditView::cmdSetPoint(float cx, float cy, int add_object) {
  Animation * animation_object;
  // First of all, clean the old group
  cx_ = cx;
  cy_ = cy;

  // Inverse Map cx, cy to world coordinates
  matrix_.imap(cx, cy);

  // Do we have anything on current point?
  animation_object = model_->inside(cx, cy);

  // If nothing at this point start selection rubberband
  if (animation_object == NULL) {
    defTag_->remove();
    startRubberBand(cx_, cy_);
    return (TCL_OK);
  }

  // Otherwise look to move object if it is a node or a Tag
  //   - A tag is a grouping marker, if an object is tagged then
  //     that tag group can be moved together
  if (animation_object->isTagged()) {
    if (animation_object->numTag() > 1) {
      fprintf(stderr, "Error: More than one tag for object %d!\n",
                       animation_object->id());
      animation_object = NULL;
    } else {
      animation_object = animation_object->getLastTag();
    }
  }
  
  // Only nodes and tags can be moved
  if ((animation_object->classid() == ClassNodeID) ||
      (animation_object->classid() == ClassTagID)) {
    
    // If a single non-tag object is selected, or explicitly 
    // instructed, remove the previous selection.  Otherwise,
    // the object under the current point will be added to the
    // entire tagged selection
    if (!add_object && (animation_object != defTag_)) {
      defTag_->remove();
    }

    // Just tags the object as being selected.
    // Draws a box around the tagged object
    startSetObject(animation_object, cx_, cy_);

  } else {
    defTag_->remove();
    startRubberBand(cx_, cy_);
  }

  return (TCL_OK);
}

//----------------------------------------------------------------------
// int
// EditView::cmdMoveTo(float cx, float cy)
//   - Moves object to end location
//   - Ends the rubber band selection
//----------------------------------------------------------------------
int EditView::cmdMoveTo(float cx, float cy) {
  // cx and cy should be in screen (canvas) coordinates
  cx_ = cx;
  cy_ = cy;

  switch (editing_stage_) {
    case START_RUBBERBAND:
    case MOVE_RUBBERBAND:
      oldx_ = rb_.xmax;
      oldy_ = rb_.ymax;
      rb_.xmax = cx_;
      rb_.ymax = cy_;
      editing_stage_ = MOVE_RUBBERBAND;
      clip_ = rb_;
      clip_.adjust();

      if (clip_.xmin > oldx_) 
        clip_.xmin = oldx_;

      if (clip_.xmax < oldx_) 
        clip_.xmax = oldx_;

      if (clip_.ymin > oldy_) 
        clip_.ymin = oldy_;

      if (clip_.ymax < oldy_) 
        clip_.ymax = oldy_;

      break;

    case START_OBJECT:
    case MOVE_OBJECT: {
      oldx_ = rb_.xmax;
      oldy_ = rb_.ymax;
      rb_.xmax = cx_;
      rb_.ymax = cy_;
      editing_stage_ = MOVE_OBJECT;
      clip_.clear();
      defTag_->merge(clip_);
      matrix_.map(clip_.xmin, clip_.ymin);
      matrix_.map(clip_.xmax, clip_.ymax);
      clip_.adjust();
      // Actual move and final bbox computation is done in render
      break;

      case START_LINK:
      case MOVE_LINK:
        // I believe this is never reached and should be removed
        // maybe it is changed in the future
        oldx_ = rb_.xmax;
        oldy_ = rb_.ymax;
        rb_.xmax = cx_;
        rb_.ymax = cy_;
        editing_stage_ = MOVE_LINK;
        clip_ = rb_;
        clip_.adjust();
        if (clip_.xmin > oldx_)
          clip_.xmin = oldx_;
        if (clip_.xmax < oldx_)
          clip_.xmax = oldx_;
        if (clip_.ymin > oldy_)
          clip_.ymin = oldy_;
        if (clip_.ymax < oldy_)
          clip_.ymax = oldy_;
        break;
      }
    default:
      // to avoid segmentation fault from click-n-dragging
      return (TCL_OK);
  }
  draw();
  return (TCL_OK);
}

int EditView::cmdReleasePoint(float cx, float cy) {
	static char buffer[512];
	int chars_written;
	
	cx_ = cx;
	cy_ = cy;
	Tcl & tcl = Tcl::instance();

	switch (editing_stage_) {
		case START_RUBBERBAND:
		case MOVE_RUBBERBAND: {
			oldx_ = rb_.xmax; oldy_ = rb_.ymax;
			rb_.xmax = cx_, rb_.ymax = cy_;
			// Need to add the region to defTag_
			clip_ = rb_;
			clip_.adjust();
			BBox bb = clip_;
			matrix_.imap(bb.xmin, bb.ymin);
			matrix_.imap(bb.xmax, bb.ymax);
			bb.adjust();
			model_->tagArea(bb, defTag_, 1);

			// We can do a total redraw
			editing_stage_ = NONE;
			draw();

			// Get time movement info for node markers on time slider
//			defTag_->addNodeMovementDestination(this, cx - oldx_, cy - oldy_, model_->now());
//			chars_written = defTag_->getNodeMovementTimes(buffer, 512);
//			if (chars_written == -1) {
				tcl.resultf("NONE");
//			} else {
//				tcl.resultf("%s", buffer);
//			}
			break;
		}

		case START_OBJECT:
		case MOVE_OBJECT: {
			oldx_ = rb_.xmax;
			oldy_ = rb_.ymax;
			rb_.xmax = cx_;
			rb_.ymax = cy_;
			clip_.clear();
			defTag_->merge(clip_);
			matrix_.map(clip_.xmin, clip_.ymin);
			matrix_.map(clip_.xmax, clip_.ymax);
			clip_.adjust();

			// Later in render() we'll compute the real bbox
			editing_stage_ = END_OBJECT;
			draw();

			editing_stage_ = NONE;
			model_->update(model_->now());
//      defTag_->move(this, rb_.xmax - oldx_, rb_.ymax - oldy_);

			// Get time movement info for node markers on time slider
			defTag_->addNodeMovementDestination(this, cx - oldx_, cy - oldy_, model_->now());
			chars_written = defTag_->getNodeMovementTimes(buffer, 512);
			if (chars_written == -1) {
				tcl.resultf("NONE");
			} else {
				tcl.resultf("%s", buffer);
			}
			break;
		}

		case START_LINK:
		case MOVE_LINK: {		
			editing_stage_ = START_LINK;
			draw();
			model_->update(model_->now());
			cmdaddLink(cx_, cy_);
			tcl.resultf("NONE");
			break;
		}

		default:
			// This cannot happen!
			editing_stage_ = NONE;
			draw();
			tcl.resultf("NONE");
	}

	return (TCL_OK);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
inline void
EditView::startRubberBand(float cx, float cy) {
	// Nothing's here. Set rubber band.
	editing_stage_ = START_RUBBERBAND;
	rb_.xmin = rb_.xmax = cx;
	rb_.ymin = rb_.ymax = cy;
	clip_ = rb_;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
inline void
EditView::startSetObject(Animation * animation_object, float cx, float cy) {
	// Find an object, add it to group
	editing_stage_ = START_OBJECT;

	// Add it into defTag_
	if (animation_object != defTag_) {
		model_->tagObject(defTag_, animation_object);
	}

	rb_.xmin = rb_.xmax = cx;
	rb_.ymin = rb_.ymax = cy;
	clip_.clear();
	defTag_->merge(clip_);

	// for the bounding box (bbox)
	oldx_ = rb_.xmax;
	oldy_ = rb_.ymax;
	rb_.xmax = cx_;
	rb_.ymax = cy_;
	clip_.clear();
	defTag_->merge(clip_);
	matrix_.map(clip_.xmin, clip_.ymin);
	matrix_.map(clip_.xmax, clip_.ymax);
	clip_.adjust();

	// Later in render() we'll compute the real bbox

	// We have selected the object and now we are at the end of 
	// the object selection process
	editing_stage_ = END_OBJECT;
	draw();

	editing_stage_ = NONE;
	model_->update(model_->now());
	editing_stage_ = START_OBJECT;
}





//----------------------------------------------------------------------
// void EditView::draw()
//----------------------------------------------------------------------
void EditView::draw() {
	if (editing_stage_ == NONE) {
		View::draw();

	} else {
		// Ignore the cleaning part
		render();
 
		// XXX Don't understand why clip's height and width need to 
		// increase 3 to draw tagged objects correctly. 
		XCopyArea(Tk_Display(tk_), offscreen_, Tk_WindowId(tk_), 
		          background_,(int)clip_.xmin, (int)clip_.ymin, 
		          (int)clip_.width()+3, (int)clip_.height()+3, 
		          (int)clip_.xmin, (int)clip_.ymin);
	}
}



//----------------------------------------------------------------------
// void EditView::draw(double current_time)
//----------------------------------------------------------------------
void EditView::draw(double current_time) {
	model_->update(current_time);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void EditView::xline(float x0, float y0, float x1, float y1, GC gc) {
  XDrawLine(Tk_Display(tk_), offscreen_, gc, (int) x0, (int) y0,
                                             (int) x1, (int) y1);
}

// Without transform.
void EditView::xrect(float x0, float y0, float x1, float y1, GC gc)
{
	int x = (int) floor(x0);
	int y = (int) floor(y0);
	int w = (int)(x1 - x0);
	if (w < 0) {
		x = (int) ceil(x1);
		w = -w;
	}
	int h = (int)(y1 - y0);
	if (h < 0) {
		h = -h;
		y = (int)ceil(y1);
	}
	XDrawRectangle(Tk_Display(tk_), offscreen_, gc, x, y, w, h);
}

void EditView::line(float x0, float y0, float x1, float y1, int color)
{
	if (editing_stage_ != NONE)
		View::line(x0, y0, x1, y1, Paint::instance()->Xor());
	else 
		View::line(x0, y0, x1, y1, color);
}

void EditView::rect(float x0, float y0, float x1, float y1, int color)
{
	if (editing_stage_ != NONE)
		View::rect(x0, y0, x1, y1, Paint::instance()->Xor());
	else 
		View::rect(x0, y0, x1, y1, color);
}

void EditView::polygon(const float* x, const float* y, int n, int color)
{
	if (editing_stage_ != NONE)
		View::polygon(x, y, n, Paint::instance()->Xor());
	else 
		View::polygon(x, y, n, color);
}

void EditView::fill(const float* x, const float* y, int n, int color)
{
	if (editing_stage_ != NONE)
		View::fill(x, y, n, Paint::instance()->Xor());
	else 
		View::fill(x, y, n, color);
}

void EditView::circle(float x, float y, float r, int color)
{
	if (editing_stage_ != NONE)
		View::circle(x, y, r, Paint::instance()->Xor());
	else 
		View::circle(x, y, r, color);
}

// Do not display any string, because no xor font gc
void EditView::string(float fx, float fy, float dim, const char* s, int anchor)
{
	if (editing_stage_ == NONE)
		View::string(fx, fy, dim, s, anchor);
}

//----------------------------------------------------------------------
// void
// EditView::BoundingBox(BBox &bb)
//   - This copies the area in which an animation drawn may be drawn
//     into (clipping area) the destination bb.  
//----------------------------------------------------------------------
void
EditView::BoundingBox(BBox &bb) {
  double temp;
  
  // Calculate the world coordinates for the view's canvas
  matrix_.imap(0.0, 0.0, bb.xmin, bb.ymin);
  matrix_.imap(width_, height_, bb.xmax, bb.ymax);

  // Check to make sure xmin, ymin is the lower left corner
  // and xmax, ymax is the upper right
  if (bb.xmin > bb.xmax) {
    temp = bb.xmax;
    bb.xmax = bb.xmin;
    bb.xmin = temp;
  }
  if (bb.ymin > bb.ymax) {
    temp = bb.ymax;
    bb.ymax = bb.ymin;
    bb.ymin = temp;
  }
}

void
EditView::getWorldBox(BBox &bb) {
  model_->BoundingBox(bb);
}

//----------------------------------------------------------------------
// void 
// EditView::render()
//   - not sure why this is called render but it seems to take care of 
//     rubberband selections and moving a selection on the screen
//   - I believe the real "rendering" is done in model_->render()
//     which is of type NetModel.  -- mehringe@isi.edu
//
//----------------------------------------------------------------------
void
EditView::render() {
  // Here we can compute the clipping box for render
  Paint *paint = Paint::instance();
  GC gc = paint->paint_to_gc(paint->Xor());

  switch (editing_stage_) {
    case START_RUBBERBAND:
      // draw rubber band
      xrect(rb_.xmin, rb_.ymin, rb_.xmax, rb_.ymax, gc);
      break;

    case MOVE_RUBBERBAND: 
      // erase previous rubberband
      xrect(rb_.xmin, rb_.ymin, oldx_, oldy_, gc);
      // draw new rubberband
      xrect(rb_.xmin, rb_.ymin, rb_.xmax, rb_.ymax, gc);
      break;

    case END_RUBBERBAND: 
      // erase previous rubber band
      xrect(rb_.xmin, rb_.ymin, oldx_, oldy_, gc);
      // XXX Should draw the tag?
      model_->render(this);
      editing_stage_ = NONE;
      break;

    case START_OBJECT:
      xrect(rb_.xmin, rb_.ymin, rb_.xmax, rb_.ymax, gc);
      // xor-draw all relevant objects
      defTag_->draw(this, model_->now());
      break;

    case MOVE_OBJECT: 
      // erase old positions first.
      if ((oldx_ == rb_.xmax) && (oldy_ == rb_.ymax)) 
        return;
      defTag_->draw(this, model_->now());
      // move these objects
      defTag_->move(this, rb_.xmax - oldx_, rb_.ymax - oldy_);
      BBox bb;
      bb.clear();
      defTag_->merge(bb);
      matrix_.imap(bb.xmin, bb.ymin);
      matrix_.imap(bb.xmax, bb.ymax);
      bb.adjust();
      clip_.merge(bb);
      defTag_->draw(this, model_->now());
      break;

    case END_OBJECT:
      // Erase old positions
      defTag_->draw(this, model_->now());
      // At least we should redo scale estimation and 
      // place everything
      defTag_->move(this, rb_.xmax - oldx_, rb_.ymax - oldy_);
      model_->recalc();
      model_->render(this);
      editing_stage_ = NONE;
      break;

    case START_LINK:
      line(link_start_x_, link_start_y_, link_end_x_, link_end_y_, 3);
      defTag_->draw(this, model_->now());
      break;

    case MOVE_LINK:
      // erase previous link
      xline(rb_.xmin, rb_.ymin, oldx_, oldy_, gc);
      // draw new rubberband
      xline(rb_.xmin, rb_.ymin, rb_.xmax, rb_.ymax, gc);
      break;

    case END_LINK:
      // erase previous link
      xline(rb_.xmin, rb_.ymin, oldx_, oldy_, gc);
      // XXX Should draw the tag?
      model_->render(this);
      editing_stage_ = NONE;
      break;

    default:
      // redraw model
      model_->render(this);
      break;
  }
  return;
}
