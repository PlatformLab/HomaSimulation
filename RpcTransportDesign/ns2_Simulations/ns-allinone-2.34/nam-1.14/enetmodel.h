/*
 * Copyright (C) 2000 by the University of Southern California
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
 * $Header: /cvsroot/nsnam/nam-1/enetmodel.h,v 1.19 2007/02/12 07:08:43 tom_henderson Exp $
 */

// Network model for Editor

#ifndef nam_enetmodel_h
#define nam_enetmodel_h

#include "netmodel.h"
#include "trafficsource.h"
#include "lossmodel.h"
#include "queuehandle.h"

// The various classifications of agents and traffic generators
// used for determining compatibility (so users can't link incompatible
// objects together in the editor)
typedef enum {
	UDP_SOURCE_AGENT, UDP_SINK_AGENT,
	TCP_SOURCE_AGENT, TCP_SINK_AGENT,
	FULLTCP_AGENT,
	UNKNOWN_AGENT
	} agent_type;

typedef enum {
	UDP_APP,
	TCP_APP,
	FULLTCP_APP,
	UNKNOWN_APP
	} trafgen_type;

// The ratio of node radius and mean edge length in the topology

class EditorNetModel : public NetModel {
public:
	EditorNetModel(const char *animator);
	virtual ~EditorNetModel();
	void BoundingBox(BBox&);

	Node * addNode(Node * node);
	Node * addNode(int node_id);
	int addNode(float cx, float cy);
	Edge* addEdge(Edge * edge);
	int addLink(Node*, Node*);
	int addLink(int source_id, int destination_id);

	agent_type classifyAgent(const char * agent);
	bool checkAgentCompatibility(agent_type source, agent_type dest);
	bool checkAgentCompatibility(const char * source, const char * dest);
	int addAgent(Agent * agent, Node * node);
	int addAgent(Node * src, const char * agent_type, float cx, float cy);
	int addAgentLink(Agent*, Agent*);
	int addAgentLink(int source_id, int destination_id);

	trafgen_type classifyTrafficSource(const char * source);
	bool checkAgentTrafficSourceCompatibility(agent_type agent, trafgen_type source);
	bool checkAgentTrafficSourceCompatibility(const char * agent, const char * source);
	int addTrafficSource(TrafficSource * traffic_source, Agent * agent);
	int addTrafficSource(Agent * agent, const char * type, float cx, float cy);
	TrafficSource * lookupTrafficSource(int id);

	int addLossModel(LossModel * loss_model, Edge * edge);
	int addLossModel(Edge * edge, const char * type, double cx, double cy);
	LossModel * lookupLossModel(int id);

	QueueHandle * addQueueHandle(Edge * edge);
	QueueHandle * addQueueHandle(Edge * edge, const char * type);
	int writeNsScript(FILE * file, const char * filename, const char * filename_root);

	void setNodeProperty(int id, const char * value, const char* variable);
	void setAgentProperty(int id, const char* value, const char* variable);
	void setLinkProperty(int source_id, int destination_id,
	                     const char * value, const char * variable);
	void setQueueHandleProperty(int source_id, int destination_id,
	                            const char * value, const char * variable);
	void setTrafficSourceProperty(int id, const char* value, const char* variable);
	void setLossModelProperty(int id, const char* value, const char * variable);
	void layout();

	void removeLink(Edge *);
	void removeNode(Node *);
	void removeAgent(Agent *a);
	void removeTrafficSource(TrafficSource * ts);
	void removeLossModel(LossModel * lossmodel);

	inline EditorNetModel* wobj() { return wobj_; }

protected:
	EditorNetModel *wobj_;
	int command(int argc, const char*const* argv);


private:
	int node_id_count;
	double	pxmin_;
	double	pymin_;
	double	pxmax_;
	double	pymax_;

	TrafficSource * traffic_sources_;
	LossModel * lossmodels_;
	QueueHandle * queue_handles_;

	double maximum_simulation_time_; //seconds
	double playback_speed_; //microseconds (us)

};


#endif // nam_enetmodel_h
