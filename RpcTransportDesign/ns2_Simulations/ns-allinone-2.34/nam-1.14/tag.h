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
 * $Header: /cvsroot/nsnam/nam-1/tag.h,v 1.4 2007/02/12 07:08:44 tom_henderson Exp $
 */

#ifndef nam_tag_h
#define nam_tag_h

#include <tcl.h>
#include "animation.h"

class EditView;

class Tag : public Animation {
public:
	Tag();
	Tag(const char *name);
	virtual ~Tag();

	void setname(const char *name);
	inline const char *name() const { return name_; }
	virtual void move(EditView *v, float wdx, float wdy);

	void add(Animation *);
	void remove(void);
	void remove(Animation *);
	void update_bb(); // Recompute bbox after a member leaves

	virtual void reset(double now);
	virtual void draw(View *, double now);
	virtual void xdraw(View *v, double now) const; // xor-draw
	virtual const char *info() const;

	inline int classid() const { return ClassTagID; }
	inline int isMember(unsigned int id) const {
		return (Tcl_FindHashEntry(mbrHash_, (const char *)id) != NULL);
	}
	inline int isMember(Animation *a) const {
		return isMember(a->id());
	}

	void addNodeMovementDestination(EditView * editview, 
	                                double dx, double dy, double current_time);
	int getNodeMovementTimes(char * buffer, int buffer_size);
	
protected:
	Tcl_HashTable *mbrHash_;
	unsigned int nMbr_;
	char *name_;
};

#endif // nam_tag_h
