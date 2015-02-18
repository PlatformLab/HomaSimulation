/*
 * Copyright (C) 1997-2001 by the University of Southern California
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
 * $Header: /cvsroot/nsnam/nam-1/enetmodel.cc,v 1.46 2007/02/12 07:08:43 tom_henderson Exp $
 */

// Network model with Editor layout

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
#include "enetmodel.h"
#include "trafficsource.h"
#include "queuehandle.h"

static int agent_number = 0;

class EditorNetworkModelClass : public TclClass {
public:
	EditorNetworkModelClass() : TclClass("NetworkModel/Editor") {}
	TclObject* create(int argc, const char*const* argv) {
		if (argc < 5) 
			return 0;
		return (new EditorNetModel(argv[4]));
	}
} editornetworkmodel_class;

EditorNetModel::EditorNetModel(const char *editor) :
	NetModel(editor) {

	node_id_count = 0;
	traffic_sources_ = NULL;
	lossmodels_ = NULL;
	queue_handles_ = NULL;
	maximum_simulation_time_ = 30.0; // seconds
	playback_speed_ = 2000.0;  // default animation speed (in microseconds)

	bind("Wpxmin_", &pxmin_);
	bind("Wpymin_", &pymin_);
	bind("Wpxmax_", &pxmax_);
	bind("Wpymax_", &pymax_);
	bind("maximum_simulation_time_", &maximum_simulation_time_);
}

EditorNetModel::~EditorNetModel()
{
}

void EditorNetModel::BoundingBox(BBox& bb)
{ 
	// by default, 800X1000 internal drawing area
	bb.xmin = pxmin_;
	bb.ymin = pymin_;
	bb.xmax = pxmax_;
	bb.ymax = pymax_;

	for (Animation* a = drawables_; a != 0; a = a->next()) {
		a->merge(bb);
	}

	pxmin_ = bb.xmin;
	pymin_ = bb.ymin;
	pxmax_ = bb.xmax;
	pymax_ = bb.ymax;
}

//----------------------------------------------------------------------
// Node * 
// EditorNetModel::addNode(Node * node)
//----------------------------------------------------------------------
Node * 
EditorNetModel::addNode(Node * node) {
	if (!lookupNode(node->num())) {
		node->next_ = nodes_;
		nodes_ = node;
		node->Animation::insert(&drawables_);
		return node;
	}
	return NULL;
}

//----------------------------------------------------------------------
// int
// EditorNetModel::addNode(int node_id)
//----------------------------------------------------------------------
Node *
EditorNetModel::addNode(int node_id) {
	Node * node = NULL;
	static char name[TRACE_LINE_MAXLEN];
	double size = 10.0; // This is the default node size taken from
											// parser.cc under the 'n' animation event

	sprintf(name, "%d", node_id);
	node = new CircleNode(name, size);		 
	node->init_color("black");
		
	if (node_id_count <= node_id) {
		node_id_count = node_id + 1;
	}

	if (!addNode(node)) {
		delete node;
		node = NULL; 
	}

	return node;
}
 

//----------------------------------------------------------------------
// int 
// EditorNetModel::addNode(float cx, float cy)
//	- Create a node using the specified name
//		and the default size and insert it into this
//		NetModel's list of drawables.
//----------------------------------------------------------------------
int 
EditorNetModel::addNode(float cx, float cy) {
	Node * node;

	// Find the next node id number
	if (node_id_count == 0 ) {
		for (node = nodes_; node != 0; node = node->next_) {
			if (node->num() > node_id_count) {
				node_id_count = node->num(); 
			}
		}
		node_id_count++;
	}
	 
	node = addNode(node_id_count);
	if (node) {
		node->place(cx,cy);
	}

	return (TCL_OK);
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
Edge *
EditorNetModel::addEdge(Edge * edge) {
	enterEdge(edge); // adds edge to a hash table

	edge->Animation::insert(&drawables_); // adds to list of drawable objects

	placeEdge(edge, edge->getSourceNode());

	return edge;
}

//----------------------------------------------------------------------
// int
// EditorNetModel::addLink(Node * src, Node * dst)
//	 - All links are duplex links by default
//----------------------------------------------------------------------
int
EditorNetModel::addLink(Node * src, Node * dst) {
	double bw = 1.0;  // Mb/s
	double delay = 20;  // ms
	double length = 0.0;
	double angle = 1.0;
	Edge * edge;

	if (!src->find_edge(dst->number()) && 
	    !dst->find_edge(src->number())) {
	
		// Create the forward edge
		edge = new Edge(src, dst, 25, bw, delay, length, angle);
		edge->init_color("black");

		enterEdge(edge);
		edge->Animation::insert(&drawables_);
		src->add_link(edge);

		placeEdge(edge, src);

		// Add queue handle for editing the forward edge's queue properties
		addQueueHandle(edge);

 
		// Create the reverse edge
		edge = new Edge(dst, src, 25, bw, delay, length, angle);

		edge->init_color("black");
		enterEdge(edge);
		edge->Animation::insert(&drawables_);
		dst->add_link(edge);

		placeEdge(edge, dst);
		
		// Add queue handle for editing the reverse edge's queue properties
		addQueueHandle(edge);
	}
	
	return (TCL_OK);
}

//----------------------------------------------------------------------
// int EditorNetModel::addLink(int source_id, int destination_id)
//----------------------------------------------------------------------
int 
EditorNetModel::addLink(int source_id, int destination_id) {
	Node * source_node, * destination_node;

	source_node = lookupNode(source_id);
	destination_node = lookupNode(destination_id);

	if (source_node && destination_node) {
		return addLink(source_node, destination_node);
	}

	return TCL_ERROR;
}


//----------------------------------------------------------------------
// agent_type
// EditorNetModel::classifyAgent(const char * agent)
//----------------------------------------------------------------------

agent_type
EditorNetModel::classifyAgent(const char * agent) {

	if (strcmp(agent, "UDP") == 0) return UDP_SOURCE_AGENT;
	if (strcmp(agent, "Null") == 0) return UDP_SINK_AGENT;
	if ((strcmp(agent, "TCP") == 0) || (strncmp(agent, "TCP/", 4) == 0))
		return TCP_SOURCE_AGENT;
	if (strcmp(agent, "TCPSink") >= 0) return TCP_SINK_AGENT;
	return UNKNOWN_AGENT;
}

//----------------------------------------------------------------------
// bool
// EditorNetModel::checkAgentCompatibility(agent_type source, agent_type dest)
//----------------------------------------------------------------------
bool
EditorNetModel::checkAgentCompatibility(agent_type source, agent_type dest) {

	return (source == UDP_SOURCE_AGENT && dest == UDP_SINK_AGENT)
		|| (source == UDP_SINK_AGENT && dest == UDP_SOURCE_AGENT)
		|| (source == TCP_SOURCE_AGENT && dest == TCP_SINK_AGENT)
		|| (source == TCP_SINK_AGENT && dest == TCP_SOURCE_AGENT)
		|| (source == FULLTCP_AGENT && dest == FULLTCP_AGENT);
}

//----------------------------------------------------------------------
// bool
// EditorNetModel::checkAgentCompatibility(const char * source, const char * dest)
//----------------------------------------------------------------------
bool
EditorNetModel::checkAgentCompatibility(const char * source, const char * dest) {

	return checkAgentCompatibility(classifyAgent(source), classifyAgent(dest));
}






//----------------------------------------------------------------------
// int
// EditorNetModel::addAgent(Agent * agent, Node * node)
//----------------------------------------------------------------------
int
EditorNetModel::addAgent(Agent * agent, Node * node) {
	placeAgent(agent, node);
		
	// Add to animations list so that it will be redrawn on all updates
	agent->Animation::insert(&animations_);
	agent->node_ = node;
	node->add_agent(agent);

	return (TCL_OK);
}

//----------------------------------------------------------------------
// int
// EditorNetModel::addAgent(Node *src, char * agent_type, float cx, float cy)
//----------------------------------------------------------------------
int
EditorNetModel::addAgent(Node * src, const char * agent_type, float cx, float cy) {
	Agent *a;
	Node *n = lookupNode(src->num());

	if (n == 0) {
		return 0;
	} else {
		a = new BoxAgent(agent_type, n->size());
		placeAgent(a, src);

		// Add to animations list so that it will be redrawn on all updates
		a->Animation::insert(&animations_);
		a->node_ = n;
		//a->number_ = agent_number++;
		agent_number++;
		n->add_agent(a);
	}
	return (TCL_OK);
}

//----------------------------------------------------------------------
// int
// EditorNetModel::addAgentLink(Agent *src_agent, Agent *dst_agent)
//	 - Need to add more constraints on which agents can connect to 
//		 each other
// 
//----------------------------------------------------------------------
int EditorNetModel::addAgentLink(Agent *src_agent, Agent *dst_agent) {

	if (!src_agent->AgentPartner_ && !dst_agent->AgentPartner_
			&& checkAgentCompatibility(src_agent->name(), dst_agent->name())) {
		// Only connect agents that don't have a partner yet
		// and are compatible with each other
	 
		agent_type src_type = classifyAgent(src_agent->name());

		if (   src_type == UDP_SOURCE_AGENT
		    || src_type == TCP_SOURCE_AGENT
		    || src_type == FULLTCP_AGENT) {

			src_agent->AgentRole_ = SOURCE;
			dst_agent->AgentRole_ = DESTINATION;
		} else {
			src_agent->AgentRole_ = DESTINATION;
			src_agent->AgentRole_ = SOURCE;
		}

		src_agent->AgentPartner_ = dst_agent;
		dst_agent->AgentPartner_ = src_agent;
		return TCL_OK;
	}

	return TCL_ERROR;
}

//----------------------------------------------------------------------
// int
// EditorNetModel::addAgentLink(int source_id, int destination_id) {
//----------------------------------------------------------------------
int
EditorNetModel::addAgentLink(int source_id, int destination_id) {
	Agent * source, * destination;

	source = lookupAgent(source_id);
	destination = lookupAgent(destination_id);

	if (source && destination) {
	 return addAgentLink(source, destination);
	}

	return TCL_ERROR;
}

//----------------------------------------------------------------------
// trafgen_type
// EditorNetModel::classifyTrafficSource(const char * source)
//----------------------------------------------------------------------
trafgen_type
EditorNetModel::classifyTrafficSource(const char * source) {
	if ( strcmp(source, "FTP") == 0
	  || strcmp(source, "Telnet") == 0) return TCP_APP;
	if ( strcmp(source, "CBR") == 0
	  || strcmp(source, "Pareto") == 0
	  || strcmp(source, "Exponential") == 0) return UDP_APP;
	return UNKNOWN_APP;
}

//----------------------------------------------------------------------
// trafgen_type
// EditorNetModel::checkAgentTrafficSourceCompatibility(agent_type agent, trafgen_type source)
//----------------------------------------------------------------------
bool
EditorNetModel::checkAgentTrafficSourceCompatibility(agent_type agent, trafgen_type source) {
	return (source == UDP_APP && agent == UDP_SOURCE_AGENT)
	    || (source == TCP_APP && agent == TCP_SOURCE_AGENT)
	    || (source == TCP_APP && agent == FULLTCP_AGENT)
	    || (source == FULLTCP_APP && agent == FULLTCP_AGENT);

}

//----------------------------------------------------------------------
// trafgen_type
// EditorNetModel::checkAgentTrafficSourceCompatibility(const char * agent, const char * source)
//----------------------------------------------------------------------
bool
EditorNetModel::checkAgentTrafficSourceCompatibility(const char * agent, const char * source) {
	
	return checkAgentTrafficSourceCompatibility(classifyAgent(agent),
			classifyTrafficSource(source));
}




//----------------------------------------------------------------------
// int
// EditorNetModel::addTrafficSource(TrafficSource * traffic_source,
//																	Agent * agent)
//----------------------------------------------------------------------
int
EditorNetModel::addTrafficSource(TrafficSource * traffic_source,
                                 Agent * agent) {

	if (traffic_source && agent
	    && checkAgentTrafficSourceCompatibility(agent->name(), traffic_source->name())) {

		traffic_source->attachTo(agent);

		// Add to animations list so that it will be redrawn on all updates
		traffic_source->Animation::insert(&animations_);

		// Add to master list of all trafficsources_ which is used to 
		// look up a traffic source for editing purposes
		traffic_source->editornetmodel_next_ = traffic_sources_;
		traffic_sources_ = traffic_source;
		return TCL_OK;
	}
	return TCL_ERROR;
}


//----------------------------------------------------------------------
// int
// EditorNetModel::addTrafficSource(Agent * agent, char * type,
//																	float cx, float cy)
//----------------------------------------------------------------------
int
EditorNetModel::addTrafficSource(Agent * agent, const char * type,
                                 float cx, float cy) {
	TrafficSource * traffic_source;
	if (agent) {
		traffic_source = new TrafficSource(type, agent->size());
		return addTrafficSource(traffic_source, agent);
	}
	return (TCL_ERROR);
}

//----------------------------------------------------------------------
// TrafficSource *
// EditorNetModel::lookupTrafficSource(int id)
//----------------------------------------------------------------------
TrafficSource *
EditorNetModel::lookupTrafficSource(int id) {
	TrafficSource * ts;

	for (ts = traffic_sources_; ts; ts = ts->editornetmodel_next_) {
		if (ts->number() == id) {
			break;
		}
	}

	return ts;
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
int
EditorNetModel::addLossModel(LossModel * loss_model, Edge * edge) {
	if (loss_model && edge) {
		edge->addLossModel(loss_model);
		loss_model->place();

		// Add to animations list so that it will be redrawn on all updates
		loss_model->Animation::insert(&animations_);

		// Add to master list of all lossmodels_ which is used to 
		// look up a lossmodel for editing purposes
		loss_model->next_lossmodel_ = lossmodels_;
		lossmodels_ = loss_model;
	}

	return TCL_OK;
}


//----------------------------------------------------------------------
//----------------------------------------------------------------------
int
EditorNetModel::addLossModel(Edge * edge, const char * type,
                             double cx, double cy) {
	LossModel * loss_model;
	if (edge && !edge->getLossModel()) {
		loss_model = new LossModel(type, edge->getSourceNode()->size());
		edge->addLossModel(loss_model);

		// Add to animations list so that it will be redrawn on all updates
		loss_model->Animation::insert(&animations_);

		// Add to master list of all lossmodels_ which is used to 
		// look up a lossmodel for editing purposes
		loss_model->next_lossmodel_ = lossmodels_;
		lossmodels_ = loss_model;
	}

	return (TCL_OK);
}

//----------------------------------------------------------------------
// TrafficSource *
// EditorNetModel::lookupLossModel(int id)
//----------------------------------------------------------------------
LossModel *
EditorNetModel::lookupLossModel(int id) {
	LossModel * loss_model;

	for (loss_model = lossmodels_; 
             loss_model;
             loss_model = loss_model->next_lossmodel_) {
		if (loss_model->number() == id) {
			break;
		}
	}

	return loss_model;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
QueueHandle * 
EditorNetModel::addQueueHandle(Edge * edge) {
	QueueHandle * queue_handle;
	
	// Add queue handle for editing the edge's queue properties
	queue_handle = new QueueHandle(edge);
	edge->addQueueHandle(queue_handle);
	queue_handle->Animation::insert(&drawables_);

	queue_handle->next_queue_handle_ = queue_handles_;
	queue_handles_ = queue_handle;

	return queue_handle;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
QueueHandle *
EditorNetModel::addQueueHandle(Edge * edge, const char * type) {
	QueueHandle * queue_handle;

	queue_handle = addQueueHandle(edge);
	queue_handle->setType(type);
	
	return queue_handle;
}


//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
void EditorNetModel::setNodeProperty(int id, const char * value, const char * variable) {
	Node * node = lookupNode(id);

	if (node) {
		if (!strcmp(variable, "size_")) {
			node->size(strtod(value,NULL));
		} else if (!strcmp(variable, "color")) {
			node->init_color(value);
		} else if (!strcmp(variable, "dlabel_")) {
			node->dlabel(value);
		} else if (!strcmp(variable, "X_")) {
			node->place(strtod(value, NULL),node->y());
		} else if (!strcmp(variable, "Y_")) {
			node->place(node->x(), strtod(value, NULL));
		} else if (!strcmp(variable, "Z_")) {
			// 
		} else {
			fprintf(stderr, "EditorNetModel::setNodeProperty - unknown property %s\n", variable);
		}
	} else {
		fprintf(stderr, "Node %d does not exist.\n", id);
     }
}

//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
void EditorNetModel::setAgentProperty(int id, const char * value, const char * variable) {
	Agent * agent = lookupAgent(id);
	if (agent) {
		if (!strcmp(variable, "windowInit_")) {
			// initial window size
			agent->windowInit(atoi(value));
		} else if (!strcmp(variable, "window_")) {
			// window size
			agent->window(atoi(value));
		} else if (!strcmp(variable, "maxcwnd_")) {
			// maximum cwnd_
			agent->maxcwnd(atoi(value));
		} else if (!strcmp(variable, "flowcolor_")) {
			// Flow ID + Color
			if ((strcmp(value, "(null)") == 0) || (strcmp(value, "") == 0)) {
				agent->flowcolor("black");
			} else {
				agent->flowcolor(value);
			} 
		} else if (!strcmp(variable, "packetSize_")) {
			agent->packetSize(atoi(value));
		} else {
//			fprintf(stderr, "EditorNetModel::setAgentProperty - unknown property %s\n", variable);
		}
	} else {
		fprintf(stderr, "EditorNetModel::setAgentProperty - Nonexisting agent %d.\n", id);
	}
}

//----------------------------------------------------------------------
// void
// EditorNetModel::setLinkProperty(int source_id, int destination_id, 
//                                 char * value, char * variable)
//----------------------------------------------------------------------
void
EditorNetModel::setLinkProperty(int source_id, int destination_id, 
                                const char * value, const char * variable) {
	Edge * edge = lookupEdge(source_id, destination_id);

	if (edge) {
		if (!strcmp(variable, "bandwidth_")) {
			edge->setBW(strtod(value,NULL));
		} else if (!strcmp(variable, "color_")) {
			edge->init_color(value);
		} else if (!strcmp(variable, "delay_")) {
			edge->setDelay(strtod(value,NULL));
		} else if (!strcmp(variable, "dlabel_")) {
			if (strcmp(value, "")) {
				edge->dlabel(value);
			}
		} else if (!strcmp(variable, "length_")) {
			edge->setLength(strtod(value,NULL));
		}
	}
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
EditorNetModel::setQueueHandleProperty(int source_id, int destination_id,
                                       const char * value, const char * variable) {
	Edge * edge;
	QueueHandle * queue;

	edge = lookupEdge(source_id, destination_id);
	if (edge) {
		queue = edge->getQueueHandle();
	 	if (queue) {
			if (strcmp(variable, "type_") == 0) {
				queue->setType(value);
			} else if (strcmp(variable, "limit_") == 0) {
				queue->setLimit(atoi(value)); 
			} else if (strcmp(variable, "secsPerByte__") == 0) {
				queue->setSecondsPerByte(strtod(value,NULL));
			} else if (strcmp(variable, "maxqueue_") == 0) {
				queue->setMaxQueue(atoi(value));
			} else if (strcmp(variable, "buckets_") == 0) {
				queue->setBuckets(atoi(value));

			} else if (strcmp(variable, "blimit_") == 0) {
				queue->setSharedBufferSize(atoi(value));

			} else if (strcmp(variable, "quantum_") == 0) {
				queue->setQuantum(atoi(value));

			} else if (strcmp(variable, "mask_") == 0) {
				if (atoi(value) == 0) {
					queue->setMask(false);
				} else {
					queue->setMask(true);
				}
			} else if (strcmp(variable, "bytes_") == 0) {
				if (atoi(value) == 0) {
					queue->setBytes(false);
				} else {
					queue->setBytes(true);
				}
			} else if (strcmp(variable, "queue_in_bytes_") == 0) {
				if (atoi(value) == 0) {
					queue->setQueueInBytes(false);
				} else {
					queue->setQueueInBytes(true);
				}
			} else if (strcmp(variable, "thresh_") == 0) {
				queue->setThreshold(strtod(value,NULL));

			} else if (strcmp(variable, "max_thresh__") == 0) {
				queue->setMaximumThreshold(strtod(value,NULL));

			} else if (strcmp(variable, "mean_pktsize_") == 0) {
				queue->setMeanPacketSize(atoi(value));

			} else if (strcmp(variable, "q_weight_") == 0) {
				queue->setQueueWeight(strtod(value,NULL));

			} else if (strcmp(variable, "wait_") == 0) {
				if (atoi(value) == 0) {
					queue->setWaitInterval(false);
				} else {
					queue->setWaitInterval(true);
				}

			} else if (strcmp(variable, "linterm_") == 0) {
				queue->setLinterm(strtod(value,NULL));
	
			} else if (strcmp(variable, "setbit_") == 0) {
				if (atoi(value) == 0) {
					queue->setBitMarking(false);
				} else {
					queue->setBitMarking(true);
				}
			} else if (strcmp(variable, "drop_tail_") == 0) {
				if (atoi(value) == 0) {
					queue->setREDDropTail(false);
				} else {
					queue->setREDDropTail(true);
				}
			}
		} else {
			fprintf(stderr, "EditorNetModel::setQueueHandleProperty() - no queuehandle on edge.\n");
		}
	}
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
EditorNetModel::setTrafficSourceProperty(int id, const char * value,
                                         const char * variable) {
	TrafficSource * trafficsource;
	
	trafficsource = lookupTrafficSource(id);
	if (trafficsource) {
		if (strcmp(variable, "start_") == 0) {
			//trafficsource->startAt(atof(value));
			trafficsource->timelist.add(strtod(value, NULL));
	
		} else if (strcmp(variable, "stop_") == 0) {
			//trafficsource->stopAt(atof(value));
			trafficsource->timelist.add(strtod(value, NULL));

		} else if (strcmp(variable, "interval_") == 0) {
			trafficsource->setInterval(atof(value));

		} else if (strcmp(variable, "maxpkts_") == 0) {
			trafficsource->setMaxpkts(atoi(value));

		} else if (strcmp(variable, "addtime") == 0) {
			trafficsource->timelist.add(atof(value));

		} else if (strcmp(variable, "removetime") == 0) {
			trafficsource->timelist.remove(atof(value));

		} else if (strcmp(variable, "timelist") == 0) {
			trafficsource->timelist.setList(value);

		// --- Exponential Properties
		} else if (strcmp(variable, "burst_time_") == 0) {
			trafficsource->setBurstTime(atoi(value));

		} else if (strcmp(variable, "idle_time_") == 0) {
			trafficsource->setIdleTime(atoi(value));

		} else if (strcmp(variable, "rate_") == 0) {
			trafficsource->setRate(atoi(value));

		// --- Pareto
		} else if (strcmp(variable, "shape_") == 0) {
			trafficsource->setShape(strtod(value, NULL));

		} else if (strcmp(variable, "title") == 0) {
		} else if (strcmp(variable, "agent_name") == 0) {
		} else {
			fprintf(stderr, " Unknown traffic source property %s\n", variable);
		}

	} else {
		fprintf(stderr, " Unable to find traffic source with id %d\n", id);
	}
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void
EditorNetModel::setLossModelProperty(int id, const char * value,
                                     const char * variable) {
	LossModel * lossmodel;
 
	lossmodel = lookupLossModel(id);
	if (lossmodel) {
		if (strcmp(variable, "period_") == 0) {
			lossmodel->setPeriod(strtod(value, NULL));
 
		} else if (strcmp(variable, "offset_") == 0) {
			lossmodel->setOffset(strtod(value, NULL));

		} else if (strcmp(variable, "burstlen_") == 0) {
			lossmodel->setBurstLength(strtod(value, NULL));

		} else if (strcmp(variable, "rate_") == 0) {
			lossmodel->setRate(strtod(value, NULL));

		} else if (strcmp(variable, "loss_unit_") == 0) {
			lossmodel->setLossUnit(value);
		}

	} else {
		fprintf(stderr, " Unable to find loss model with id %d\n", id);
	}
}


//----------------------------------------------------------------------
//
//----------------------------------------------------------------------
int EditorNetModel::command(int argc, const char *const *argv) {
	FILE * file;
	Node * node;
	double time;

	if (argc == 2) {
		if (strcmp(argv[1], "layout") == 0) {
			layout();
			return (TCL_OK);
		}

	} else if (argc == 4) {
		// Write out ns script information
		if (strcmp(argv[1], "saveasns") == 0) {
			// argv syntax is as follows:
			// <net> saveasns filename filename_root
			file = fopen(argv[2], "w");
			if (file) {
//				fprintf(stderr, "Writing ns script to %s...",argv[2]);
				writeNsScript(file, argv[2], argv[3]);
				fclose(file);
//				fprintf(stderr, "done.\n");
			} else {
				fprintf(stderr, "nam: Unable to open file: %s\n", argv[2]);
			}
			return (TCL_OK);

		} else if (strcmp(argv[1], "removeNodeMovement") == 0) {
			node = lookupNode(atoi(argv[2]));
               time = strtod(argv[3], NULL);
			node->removeMovementDestination(time);
			return TCL_OK;
		} 
	}			
	return (NetModel::command(argc, argv));
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void EditorNetModel::layout() {
	Node *n;
	for (n = nodes_; n != 0; n = n->next_)
		for (Edge* e = n->links(); e != 0; e = e->next_)
			placeEdge(e, n);
}

//----------------------------------------------------------------------
// int 
// EditorNetModel::saveAsNs(FILE * file)
//----------------------------------------------------------------------
int 
EditorNetModel::writeNsScript(FILE * file, const char * filename, const char * filename_root) {
	Node * node;
	Edge * edge;
	LossModel * loss_model;	
	Agent * agent;
	int number_of_wireless_nodes = 0;
	int number_of_wired_nodes = 0;
	double maximum_x, maximum_y;
	double node_x, node_y;

	maximum_x = 500;
	maximum_y = 500;
	
	// Add a disclaimer

	fprintf(file, "#------------------------------------------------------- \n");
	fprintf(file, "# This ns script has been created by the nam editor.\n");
	fprintf(file, "# If you edit it manually, the nam editor might not\n");
	fprintf(file, "# be able to open it properly in the future.\n");
	fprintf(file, "#\n");
	fprintf(file, "# EDITING BY HAND IS AT YOUR OWN RISK!\n");
	fprintf(file, "#------------------------------------------------------- \n");

	fprintf(file, "# Create a new simulator object.\n");
	fprintf(file, "set ns [new Simulator]\n");
	fprintf(file, "# Create a nam trace datafile.\n");
	fprintf(file, "set namfile [open %s.nam w]\n", filename_root);
	fprintf(file, "$ns namtrace-all $namfile\n");

	// write out node creation infomation
	fprintf(file, "\n# Create wired nodes.\n");
	for (node = nodes_; node; node = node->next_) {
		if (node->links()) {
			// Only write out wired links here
			node->writeNsScript(file);
			fprintf(file, "\n");
			number_of_wired_nodes++;
		} else {
			// If a node has no links it must be a wireless node
			number_of_wireless_nodes++;
		}

		// Find maximum node mobility and placement range
		node_x = node->getMaximumX();
		node_y = node->getMaximumY();

		if (node_x > maximum_x) {
			// Add some extra space to be sure node is within
			// wireless boundary
			maximum_x = node_x + 2.0*node->size();
		}

		if (node_y > maximum_y) {
			// Add some extra space to be sure node is within
			// wireless boundary
			maximum_y = node_y + 2.0*node->size();
		}
	}

	if (number_of_wireless_nodes > 0) {
		fprintf(file, "\n# ----- Setup wireless environment. ----\n");
		fprintf(file, "set wireless_tracefile [open %s.trace w]\n", filename_root);

		fprintf(file, "set topography [new Topography]\n");

		fprintf(file, "$ns trace-all $wireless_tracefile\n");
		fprintf(file, "$ns namtrace-all-wireless $namfile %f %f\n", maximum_x, maximum_y);

		fprintf(file, "$topography load_flatgrid %f %f\n", maximum_x, maximum_y);

		fprintf(file, "#\n");
		fprintf(file, "# Create God\n");
		fprintf(file, "#\n");
		fprintf(file, "set god_ [create-god %d]\n", number_of_wireless_nodes);


		fprintf(file, "#global node setting\n");

		fprintf(file, "$ns node-config -adhocRouting DSR \\\n");
		fprintf(file, "                 -llType LL \\\n");
		fprintf(file, "                 -macType Mac/802_11 \\\n");
		fprintf(file, "                 -ifqType Queue/DropTail/PriQueue \\\n");
		fprintf(file, "                 -ifqLen 50 \\\n");
		fprintf(file, "                 -antType Antenna/OmniAntenna \\\n");
		fprintf(file, "                 -propType Propagation/TwoRayGround \\\n");
		fprintf(file, "                 -phyType Phy/WirelessPhy \\\n");
		fprintf(file, "                 -channel [new Channel/WirelessChannel] \\\n");
		fprintf(file, "                 -topoInstance $topography \\\n");
		fprintf(file, "                 -agentTrace ON \\\n");
		fprintf(file, "                 -routerTrace OFF \\\n");
		fprintf(file, "                 -macTrace ON\n");


		fprintf(file, "\n# Create wireless nodes.\n");

		for (node = nodes_; node; node = node->next_) {
			if (!node->links()) {
				node->writeNsScript(file);
				node->writeNsMovement(file);
			}
		}
	}

	// write out link information
	fprintf(file, "\n# Create links between nodes.\n");
	for (node = nodes_; node ; node = node->next_) {
		for (edge = node->links(); edge; edge = edge->next_) {
			edge->writeNsScript(file);
		}
	}
	
	// Write out lossmodel information
	fprintf(file, "# Add Link Loss Models\n");
	for (loss_model = lossmodels_;
		loss_model;
		loss_model = loss_model->next_lossmodel_) {
		loss_model->writeNsDefinitionScript(file);
	}


	// write out agents and it's traffic sources
	fprintf(file, "\n# Create agents.\n");
	for (node = nodes_; node; node = node->next_) {
		for (agent = node->agents(); agent; agent = agent->next_) {
			agent->writeNsDefinitionScript(file);
		}
	}

	// Write out agent connections
	// An agent can connect to only one other agent
	fprintf(file, "\n# Connect agents.\n");
	for (node = nodes_; node; node = node->next_) {
		for (agent = node->agents(); agent; agent = agent->next_) {
			agent->writeNsConnectionScript(file);
		}
	}

	// Find time at which to stop the simulation
//	for (ts = traffic_sources_; ts; ts = ts->editornetmodel_next_) {
//		if (finish_time < ts->stopAt()) {
//			finish_time = ts->stopAt();
//		}
//	}
		

	fprintf(file, "# Run the simulation\n");
	fprintf(file, "proc finish {} {\n	global ns namfile\n	$ns flush-trace\n");
	fprintf(file, "	close $namfile\n	exec nam -r %fus %s.nam &	\n	exit 0\n	}\n", playback_speed_, filename_root);
	//fprintf(file, "$ns at %f \"finish\"\n", finish_time);
	fprintf(file, "$ns at %f \"finish\"\n", maximum_simulation_time_);
	fprintf(file, "$ns run\n");
	return 0;
}

//----------------------------------------------------------------------
// void
// EditorNetModel::removeNode(Node *n)
//	 - remove node n from the nodes_ list and delete it
//----------------------------------------------------------------------
void EditorNetModel::removeNode(Node *n) {
	Node * previous, * run;
	Edge * edge;
	Agent * agent;
	previous = nodes_;		 

	for (run = nodes_; run; run = run->next_) {
		if (n->num() == run->num()) {
			// when deleting the last node
			if (run->num() == nodes_->num()) {
				nodes_ = nodes_->next_;
				break;
			} else {
				previous->next_ = run->next_; 
				break;
			}
		}
		previous = run;
	}
	run->next_ = NULL;

	// Remove it from list of drawables
	run->detach();	

	// delete edges of the nodes_
	for (edge = run->links(); edge != 0; edge = edge->next_) {
		removeLink(edge);
	}

	// delete agents on the nodes_
	for (agent = run->agents(); agent != 0; agent = agent->next_) {
		removeAgent(agent);
	}

	delete run;
}

//----------------------------------------------------------------------
// void
// EditorNetModel::removeLink(Edge *e)
// 
//----------------------------------------------------------------------
void
EditorNetModel::removeLink(Edge *e) {
	EdgeHashNode * h, * g;
	Edge * e1, * e2;
     LossModel * lossmodel;
	int src = e->src();
	int dst = e->dst();

	h = lookupEdgeHashNode(src, dst);
	g = lookupEdgeHashNode(dst, src);

	if (h == 0 || g == 0) {
		// h,g = 0 or h,g !=0
		return;
	} 

	e1 = h->edge;
	e2 = g->edge;

     lossmodel = e1->getLossModel();
     if (lossmodel) {
       removeLossModel(lossmodel);
     }
 
     lossmodel = e2->getLossModel();
     if (lossmodel) {
       removeLossModel(lossmodel);
     }
     
	// defined in netmodel.cc
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

//----------------------------------------------------------------------
// void 
// EditorNetModel::removeAgent(Agent *a)
//	 - removes and deletes an Agent and all of its Traffic Sources
//----------------------------------------------------------------------
void 
EditorNetModel::removeAgent(Agent *a) {
	Node * n;
	TrafficSource * ts, * next_ts;
	n = a->node_;
	
	if (a->AgentPartner_ != NULL) {
		a->AgentPartner_->AgentPartner_ = NULL;
		a->AgentPartner_->AgentRole_ = 0;
	}
	n->delete_agent(a);
	a->detach();

	for (ts = a->traffic_sources_; ts; ts = next_ts) {
		// Save next traffic source in list since this
		// traffic source will be deleted in removeTrafficSource
		next_ts = ts->next_;
		removeTrafficSource(ts);
	}

	delete a;
}


//----------------------------------------------------------------------
// void 
// EditorNetModel::removeTrafficSource(TrafficSource * ts) {
//----------------------------------------------------------------------
void 
EditorNetModel::removeTrafficSource(TrafficSource * ts) {
	TrafficSource * run, * previous;

	// Run down the list of traffic sources
	previous = NULL;
	for (run = traffic_sources_; run; run = run->editornetmodel_next_) {
		if (run == ts) {
			break;
		}
		previous = run;
	}

	if (previous) {
		previous->editornetmodel_next_ = ts->editornetmodel_next_;
	} else {
		if (run == traffic_sources_) {
			// ts is at the front of the list
			traffic_sources_ = ts->editornetmodel_next_;
		}
	}
	ts->editornetmodel_next_ = NULL;

	ts->removeFromAgent();

	delete ts; 
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void 
EditorNetModel::removeLossModel(LossModel * lossmodel) {
	LossModel * run, * previous;

	// Run down the list of lossmodels
	previous = NULL;
	for (run = lossmodels_; run; run = run->next_lossmodel_) {
		if (run == lossmodel) {
			break;
		}
		previous = run;
	}

	if (previous) {
		previous->next_lossmodel_ = lossmodel->next_lossmodel_;
	} else {
		if (run == lossmodels_) {
			// ts is at the front of the list
			lossmodels_ = lossmodel->next_lossmodel_;
		}
	}
	lossmodel->next_lossmodel_ = NULL;

	lossmodel->clearEdge();

	delete lossmodel;
} 
