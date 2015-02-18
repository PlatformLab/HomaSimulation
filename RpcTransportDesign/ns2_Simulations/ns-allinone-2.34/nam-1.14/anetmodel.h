/*
 * Network model with automatic layout
 *
 * $Header: /cvsroot/nsnam/nam-1/anetmodel.h,v 1.12 2000/11/12 23:19:39 mehringe Exp $
 */

#ifndef nam_anetmodel_h
#define nam_anetmodel_h

#include "netmodel.h"

class AutoNetModel : public NetModel {
public:
	AutoNetModel(const char *animator);
	virtual ~AutoNetModel();

	static int RANDOM_SEED_;
	static int AUTO_ITERATIONS_;
	static int INCR_ITERATIONS_;
	static int recalc_;
	static double INIT_TEMP_; // follow Elan's 
	static double MINX_; 
	static double MAXX_;
	static double MINY_;
	static double MAXY_;
	static double min(double a, double b) {
		return (a < b) ? a : b;
	}
	static double max(double a, double b) {
		return (a > b) ? a : b;
	}

	void handle(const TraceEvent& e, double now, int direction);
	virtual void recalc() {
		scale_estimate();
		placeEverything();
	}
	//virtual void moveNode(Node *n);

  void relayoutNode(Node * v);

protected:
	void reset_layout();
	void layout();
	void relayout();
	virtual void embed_phase1();
	virtual void embed_phase2();
	virtual void embed_phase1(Node * v);
	virtual void embed_phase2(Node * v);
	virtual void scale_estimate();
	virtual void placeEverything();
	void placeAllAgents(Node *src) const;
        void weigh_subtrees();
        void place_subtrees();
        void bifucate_graph();
        int mark_to_depth(Node *n, int depth);

	// attractive and repulsive forces
	double ATT (double d, double k) { return d * d / k; }
	double ATT2 (double d, double k) { return d * d * d / k; }
	double REP (double d, double k) { return k * k / d; }
	void cool();
	void initEmbedding();
	
	// we need our own 'cmd layout'
	int command(int argc, const char*const* argv); 

private:
	int iterations_;
	int nNodes_;
	double optk_;    // k used in computing att/rep forces
	double optka_;
	double optkr_;
	double temp_;
	double kc_;
	double kca_;
	double kcr_;
};


#endif // nam_anetmodel_h
