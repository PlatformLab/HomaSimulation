/*
 * Network model with wireless net layout
 *
 */

#ifndef nam_wnetmodel_h
#define nam_wnetmodel_h

#include "netmodel.h"

// The ratio of node radius and mean edge length in the topology

class WirelessNetModel : public NetModel {
public:
	WirelessNetModel(const char *animator);
	virtual ~WirelessNetModel();

	void handle(const TraceEvent& e, double now, int direction);
	void removeLink(const TraceEvent& e);
	void removeLink(int src, int dst );
	void BoundingBox(BBox&);
	virtual void moveNode(Node *n);
	inline WirelessNetModel* wobj() { return wobj_; }
	virtual void update(double);
	       
	void addToEdgePool(Edge*);
	Edge* removeFromEdgePool();

protected:
	void layout();
	virtual void placeEdge(Edge* e, Node* src) const;
	WirelessNetModel *wobj_;

	// we need our own 'cmd layout'
	int command(int argc, const char*const* argv); 

struct WEdgeList {
       Edge* wEdge;
       WEdgeList* next;
};      
		 

private:
	int nNodes_;
	double	pxmax_;
	double  pymax_;
        WEdgeList* edges_;
};


#endif // nam_wnetmodel_h
