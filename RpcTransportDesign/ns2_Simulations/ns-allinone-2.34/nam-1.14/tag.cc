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
 * $Header: /cvsroot/nsnam/nam-1/tag.cc,v 1.7 2007/02/12 07:08:43 tom_henderson Exp $
 */

#ifdef WIN32
#include <windows.h>
#endif
#include <string.h>
#include <tcl.h>
#include "animation.h"
#include "tag.h"
#include "view.h"
#include "node.h"
#include "editview.h"

Tag::Tag()
	: Animation(0, 0), nMbr_(0), name_(NULL)
{
	mbrHash_ = new Tcl_HashTable;
	Tcl_InitHashTable(mbrHash_, TCL_ONE_WORD_KEYS);
}

Tag::Tag(const char *name)
	: Animation(0, 0), nMbr_(0)
{
	size_t len = strlen(name);
	if (len == 0) 
		name_ = NULL;
	else {
		name_ = new char[len+1];
		strcpy(name_, name);
	}
	mbrHash_ = new Tcl_HashTable;
	Tcl_InitHashTable(mbrHash_, TCL_ONE_WORD_KEYS);
}

Tag::~Tag()
{
	remove();
	Tcl_DeleteHashTable(mbrHash_);
	delete mbrHash_;	
	if (name_ != NULL)
		delete []name_;
}

void Tag::setname(const char *name)
{
	if (name_ != NULL)
		delete name_;
	size_t len = strlen(name);
	if (len == 0) 
		name_ = NULL;
	else {
		name_ = new char[len+1];
		strcpy(name_, name);
	}
}

void Tag::add(Animation *a)
{
	int newEntry = 1;
	Tcl_HashEntry *he = 
		Tcl_CreateHashEntry(mbrHash_,(const char *)a->id(), &newEntry);
	if (newEntry && (he != NULL)) {
		// Do not handle exception he == NULL. Don't know how.
		Tcl_SetHashValue(he, (ClientData)a);
		nMbr_++;
		a->merge(bb_);
		// Let that object know about this group
		a->addTag(this); 
	}
}

void Tag::remove()
{
	// Remove this tag from all its members
	Animation *p;
	Tcl_HashEntry *he;
	Tcl_HashSearch hs;
	int i = 0;
	for (he = Tcl_FirstHashEntry(mbrHash_, &hs);
	     he != NULL;
	     he = Tcl_NextHashEntry(&hs), i++) {
		p = (Animation *) Tcl_GetHashValue(he);
		p->deleteTag(this);
		Tcl_DeleteHashEntry(he);
		nMbr_--;
	}
	bb_.clear();
}

void Tag::remove(Animation *a)
{
	Tcl_HashEntry *he = Tcl_FindHashEntry(mbrHash_, (const char *)a->id());
	if (he != NULL) {
		a->deleteTag(this);
		Tcl_DeleteHashEntry(he);
		nMbr_--;
		update_bb();
	}
}

void Tag::draw(View *v, double now) {
	// XXX don't draw anything except it's selected. Or draw 
	// 4 black dots in the 4 corners.
	v->rect(bb_.xmin, bb_.ymin, bb_.xmax, bb_.ymax, 
		Paint::instance()->thin());
}

// Used for xor-draw only
void Tag::xdraw(View *v, double now) const 
{
	// Draw every object of this group
	Tcl_HashEntry *he;
	Tcl_HashSearch hs;
	Animation *a;

	for (he = Tcl_FirstHashEntry(mbrHash_, &hs);
	     he != NULL;
	     he = Tcl_NextHashEntry(&hs)) {
		a = (Animation *) Tcl_GetHashValue(he);
		a->draw(v, now);
	}
}

void Tag::update_bb(void)
{
	Tcl_HashEntry *he;
	Tcl_HashSearch hs;
	Animation *a;

	bb_.clear();
	for (he = Tcl_FirstHashEntry(mbrHash_, &hs);
	     he != NULL;
	     he = Tcl_NextHashEntry(&hs)) {
		a = (Animation *) Tcl_GetHashValue(he);
		a->merge(bb_);
	}
}

void Tag::reset(double /*now*/)
{
}

//----------------------------------------------------------------------
// void 
// Tag::move(EditView *v, float wdx, float wdy)
//   -- Move all tagged object and update the bounding box
//----------------------------------------------------------------------
void
Tag::move(EditView *v, float wdx, float wdy) {
	Tcl_HashEntry *he;
	Tcl_HashSearch hs;
	Animation *a;

	bb_.clear();
	for (he = Tcl_FirstHashEntry(mbrHash_, &hs);
	     he != NULL;
	     he = Tcl_NextHashEntry(&hs)) {
		a = (Animation *) Tcl_GetHashValue(he);
		a->move(v, wdx, wdy);

		a->merge(bb_);
	}
}

const char *Tag::info() const
{
	static char str[256];
	strcpy(str, "Tag");
	return str;
}

//----------------------------------------------------------------------
// void
// Tag::addNodeMovementElements(EditView * editview, 
//                              double x, double y, double current_time)
//   - Used by the nam editor
//   - If any nodes in the tagged selection are mobile then the
//----------------------------------------------------------------------
void
Tag::addNodeMovementDestination(EditView * editview, 
                                double dx, double dy, double current_time) {
	Tcl_HashEntry * hash_entry;
	Tcl_HashSearch hash_search;
	Animation * animation_object;
	Node * node;
	float x,y;


	for (hash_entry = Tcl_FirstHashEntry(mbrHash_, &hash_search);
	     hash_entry != NULL;
	     hash_entry = Tcl_NextHashEntry(&hash_search)) {
		animation_object = (Animation *) Tcl_GetHashValue(hash_entry);

		if (animation_object->classid() == ClassNodeID) {
			node = (Node *) animation_object;
			x = node->x();
			y = node->y();

			editview->map(x,y);
			x += dx;
			y += dy;
			editview->imap(x,y);

			node->addMovementDestination(x, y, current_time);

		}
	}
}


//----------------------------------------------------------------------
// int
// Tag::getNodeMovementTimes(char * buffer, int buffer_size)
//   - returns a string of the form
//     {{<node id> <time> <time_2>...} {node_id2 <time> <time2>...}...}
//----------------------------------------------------------------------
int
Tag::getNodeMovementTimes(char * buffer, int buffer_size) {
	Tcl_HashEntry * hash_entry;
	Tcl_HashSearch hash_search;
	Animation * animation_object;
	Node * node;
	int chars_written, just_written;

	chars_written = 0;
	for (hash_entry = Tcl_FirstHashEntry(mbrHash_, &hash_search);
	     hash_entry != NULL;
	     hash_entry = Tcl_NextHashEntry(&hash_search)) {

		animation_object = (Animation *) Tcl_GetHashValue(hash_entry);

		if (animation_object->classid() == ClassNodeID) {
			node = (Node *) animation_object;
			if (chars_written  < buffer_size) {
				just_written = sprintf(buffer + chars_written, "{%d ", node->number());
				if (just_written != -1) {
					chars_written += just_written;
					chars_written += node->getMovementTimeList(buffer + chars_written,
					                                           buffer_size - chars_written) -1;
					chars_written += sprintf(buffer + chars_written, "}");
				} else {
					chars_written = -1;
					break;
				}
			}
		}
		chars_written += sprintf(buffer + chars_written, " ");
	}
	return chars_written;
}
