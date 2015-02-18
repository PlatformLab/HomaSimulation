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
 * $Header: /cvsroot/nsnam/nam-1/group.cc,v 1.5 2007/02/12 07:08:43 tom_henderson Exp $
 */

#include <string.h>
#include "group.h"

Group::Group(const char *name, unsigned int addr) :
	Animation(0, 0), size_(0), addr_(addr), name_(0)
{
	if (name != NULL) 
		if (*name != 0) {
			name_ = new char[strlen(name)+1];
			strcpy(name_, name);
		}
	nodeHash_ = new Tcl_HashTable;
	Tcl_InitHashTable(nodeHash_, TCL_ONE_WORD_KEYS);
}

Group::~Group()
{
	if (name_ != NULL)
		delete name_;
	Tcl_DeleteHashTable(nodeHash_);
	delete nodeHash_;
}

int Group::join(int id)
{
	int newEntry = 1;
	Tcl_HashEntry *he = Tcl_CreateHashEntry(nodeHash_, (const char *)id, 
						&newEntry);
	if (he == NULL)
		return -1;
	if (newEntry) {
		Tcl_SetHashValue(he, (ClientData)id);
		size_++;
	}
	return 0;
}

void Group::leave(int id)
{
	Tcl_HashEntry *he = Tcl_FindHashEntry(nodeHash_, (const char *)id);
	if (he != NULL) {
		Tcl_DeleteHashEntry(he);
		size_--;
	}
}

// Assume mbrs has at least size_ elements
void Group::get_members(int *mbrs)
{
	Tcl_HashEntry *he;
	Tcl_HashSearch hs;
	int i = 0;
	for (he = Tcl_FirstHashEntry(nodeHash_, &hs);
	     he != NULL;
	     he = Tcl_NextHashEntry(&hs), i++) 
		mbrs[i] = *Tcl_GetHashValue(he);
}

void Group::draw(View * nv, double now) {
	// Do nothing for now. Will add group visualization later.
}
