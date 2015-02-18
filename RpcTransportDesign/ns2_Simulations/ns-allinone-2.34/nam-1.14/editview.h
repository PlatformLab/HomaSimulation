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
 * $Header: /cvsroot/nsnam/nam-1/editview.h,v 1.19 2007/02/12 07:08:43 tom_henderson Exp $
 */

#ifndef nam_EditView_h
#define nam_EditView_h

extern "C" {
#include <tcl.h>
#include <tk.h>
}

#include "tkcompat.h"
#include "enetmodel.h"
#include "transform.h"
#include "view.h"
#include "netview.h"
#include "bbox.h"
#include "tag.h"
#include "trafficsource.h"

class NetModel;
struct TraceEvent;
class Tcl;
class Paint;

class EditView : public NetView {
public:
	EditView(const char* name, NetModel *m);
 	EditView(const char* name, NetModel *m, int height, int width);
	virtual ~EditView();

	static int command(ClientData, Tcl_Interp*, int argc, CONST84 char **argv);
	static void DeleteCmdProc(ClientData);

	virtual void draw();
	virtual void draw(double current_time);
	virtual void render();
	virtual void BoundingBox(BBox &bb);
	virtual void getWorldBox(BBox & world_boundary);

	int cmdSetPoint(float, float, int);
	int cmdMoveTo(float, float);
	int cmdReleasePoint(float, float);
	int cmdRemoveSel(float, float);
	int cmdaddNode(float, float);
	int cmdaddLink(float, float);
	int cmdaddAgent(float, float, const char*);
	int showAgentLink(float cx, float cy);
	int cmdaddAgentLinker(float, float);
	int cmdaddTrafficSource(float cx, float cy, const char * type);
	int cmdaddLossModel(float cx, float cy, const char * type);
	int cmdgetObjectInformation(float cx, float cy);
	int cmdgetObjProperty(float cx, float cy);
	int cmdgetObjectProperties(float cx, float cy, const char * type);
	int cmdsetNodeProperty(int id, 
			       const char * value, const char * variable);
	int cmdsetAgentProperty(int id, 
				const char * value, const char * variable);
	int cmdsetLinkProperty(int sid, int did,
	                       const char * value, const char * variable);
	int cmdsetTrafficSourceProperty(int id, const char * value, const char * variable);
	int cmdsetLossModelProperty(int id, const char * value, const char * variable);
	int cmdDeleteObj(float, float);

	void moveNode(Node *n) const {
		model_->moveNode(n);
	}

	virtual void line(float x0, float y0, float x1, float y1, int color);
	virtual void rect(float x0, float y0, float x1, float y1, int color);
	virtual void polygon(const float* x, const float* y, int n, int color);
	virtual void fill(const float* x, const float* y, int n, int color);
	virtual void circle(float x, float y, float r, int color);
	virtual void string(float fx, float fy, float dim, const char* s, int anchor);
	void view_mode() {
		defTag_->remove();
		draw();
	}

protected:
	// xor-draw rectangle
	void xrect(float x0, float y0, float x1, float y1, GC gc);
	void xline(float x0, float y0, float x1, float y1, GC gc);

	enum EditType { START_RUBBERBAND, MOVE_RUBBERBAND, END_RUBBERBAND,
			START_OBJECT, MOVE_OBJECT, END_OBJECT, NONE,
			START_LINK, MOVE_LINK, END_LINK };

	inline void startRubberBand(float cx, float cy); 

	inline void startSetObject(Animation *p, float cx, float cy);

	// Grouping/interactive stuff
	Tag * defTag_;  // This is a tag object that is used for adding
	                // animation object into a selection group so that
	                // they can be moved together
                
	Animation * curObj_;
	float cx_, cy_; 	// current point
	EditType editing_stage_;	// current selection type
	BBox rb_;		// rubber band rectangle
	float oldx_, oldy_;	// old rubber band position, used for erasion
	float link_start_x_, link_start_y_;       // link origin
	float link_end_x_, link_end_y_;           // link destination
};

#endif
