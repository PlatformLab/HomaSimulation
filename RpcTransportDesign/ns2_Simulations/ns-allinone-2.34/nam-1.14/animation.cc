/*
 * Copyright (c) 1991,1993 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
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
 * @(#) $Header: /cvsroot/nsnam/nam-1/animation.cc,v 1.24 2003/10/11 22:56:49 xuanc Exp $ (LBL)
 */

#include <memory.h>
#include <tcl.h>
#include <math.h>
#include <string.h>
#include "animation.h"
#include "paint.h"
#include "monitor.h"

unsigned int Animation::LASTID_ = 0;
Tcl_HashTable *Animation::AniHash_ = 0;
unsigned int Animation::nAniHash_ = 0;

// Static method
Animation* Animation::find(unsigned int id)
{
	Tcl_HashEntry *he = Tcl_FindHashEntry(AniHash_, (const char *)id);
	Animation *res = NULL;
	if (he != NULL) 
		res = (Animation*) Tcl_GetHashValue(he);
	return res;
}

Animation::Animation(double tim, long offset)
{
	paint_ = 0;
	oldPaint_ = 0;
	next_ = 0;
	prev_ = 0;
	si_.time = tim;
	si_.offset = offset;
	monitor_ = NULL;
	id_ = Animation::LASTID_++;
	tags_ = NULL;
	nTag_ = 0;
	aType_ = 0 ;
	bb_.clear();

	if (Animation::AniHash_ == 0) {
		Animation::AniHash_ = new Tcl_HashTable;
		Tcl_InitHashTable(Animation::AniHash_, TCL_ONE_WORD_KEYS);
	}

	// Enter hash table for quick lookup and access
	int newEntry = 1;
	Tcl_HashEntry *he = Tcl_CreateHashEntry(AniHash_, 
						(const char *)id_, &newEntry);
	if (newEntry && (he != NULL)) {
		// Do not handle exception he == NULL. Don't know how.
		Tcl_SetHashValue(he, (ClientData)this);
		nAniHash_++;
	}
}

//----------------------------------------------------------------------
// Animation::~Animation()
//----------------------------------------------------------------------
Animation::~Animation() {
//	if (prev_ != 0)
//		*prev_ = next_;
//	if (next_ != 0)
//		next_->prev_ = prev_;
// The above is taken care of by calling detach
  // Remove myself from the list on which I may be connected
  detach();

	if (tags_ != NULL)
		delete tags_;

	// Remove from hash table
	Tcl_HashEntry *he = Tcl_FindHashEntry(AniHash_, (const char *)id_);
	if (he != NULL) {
		Tcl_DeleteHashEntry(he);
		nAniHash_--;
	}
}

void Animation::addTag(Animation *t)
{
	if (tags_ == NULL) 
		tags_ = new Animation* [AnimationTagIncrement];
	else if (nTag_ % AnimationTagIncrement == 0) {
		// Increase tag array space
		Animation **tmp = new Animation* [nTag_+AnimationTagIncrement];
		memcpy((char *)tmp, (char *)tags_, nTag_ * sizeof(Animation*));
		delete tags_;
		tags_ = tmp;
	}
	tags_[nTag_++] = t;
}

void Animation::deleteTag(Animation *t)
{
	for (int i = 0; i < nTag_; i++) {
		if (tags_[i] == t) {
			tags_[i] = tags_[nTag_ - 1];
			nTag_ --;
		}
	}
}

//----------------------------------------------------------------------
// void
// Animation::detach()
//   - Removes animation object from its prev_/next_ linked list
//   - Mostly used by the drawables_ and animations_ lists in NetModel
//----------------------------------------------------------------------
void
Animation::detach() {
	if (prev_ != 0) {
    // Set my previous neighbor's next_ to point to my next neighbor
		*prev_ = next_;
  }
  
	if (next_ != 0) {
    // Set my next neighbor's prev_ to point to my previous neighbors next_
		next_->prev_ = prev_;
  }

  // When you remove this object don't forget to set all animation 
  // pointers to NULL because when this object is deleted it will also 
  // try to detach itself from the list
  prev_ = NULL;
  next_ = NULL;
}

//----------------------------------------------------------------------
// void
// Animation::insert(Animation **head)
//   - Insert to the front of the list
//   - This seems to be a bad implementation.  All over the nam code
//     I see use of this insert method right after the next_ pointer
//     is used for another list.
//     For Example from netmodel.cc:
//               n->next_ = nodes_;
//               nodes_ = n;
//               n->insert(&drawables_);
//
//   - It works but is very confusing as to which next_ refers to what.
//----------------------------------------------------------------------
void
Animation::insert(Animation **head) {
  //Set my next pointer to the former head of the list
	next_ = * head;

	if (next_) {
    // Make the former head of the list point to my next_
    // so that it can assign me to another object if it is 
    // removed.
		next_->prev_ = &next_;
  }

	prev_ = head;
	*head = this;
}

void Animation::update(double /*now*/)
{
	/* do nothing */
}

void Animation::reset(double /*now*/)
{
	/* do nothing */
}

int Animation::inside(double /*now*/, float px, float py) const
{
	// Why not use bbox to make a generic inside???
	return bb_.inside(px, py);
}

float Animation::distance(float, float) const
{
	// do nothing 
	return HUGE_VAL;
}

void
Animation::merge(BBox & b) { 
	if (b.xmin > bb_.xmin)
		b.xmin = bb_.xmin;

	if (b.ymin > bb_.ymin)
		b.ymin = bb_.ymin;

	if (b.xmax < bb_.xmax)
		b.xmax = bb_.xmax;

	if (b.ymax < bb_.ymax)
		b.ymax = bb_.ymax;
}


const char* Animation::info() const
{
	return (0);
}

const char* Animation::property()
{
	return (0);
}

//----------------------------------------------------------------------
// const char *
// Animation::getProperties(char * type)
//   - type is a object type. for example in queuehandle.h you can 
//     have different types of queueahandles (DropTail, RED, etc)
//   - Used by the editor to update the properties window when the
//     type is changed
//----------------------------------------------------------------------
const char *
Animation::getProperties(const char * type) {
	return property();
}

const char* Animation::getname() const
{
	return (0);
}

const char* Animation::getfid() const
{
	return (0);
}

const char* Animation::getesrc() const
{
	return (0);
}

const char* Animation::getedst() const
{
	return (0);
}

const char* Animation::gettype() const
{
	return (0);
}

void Animation::monitor(Monitor *m, double now, char *result, int len)
{
}

MonState *Animation::monitor_state(void)
{
  return NULL;
}


void Animation::color(const char *color)
{
  int pno = Paint::instance()->lookup(color, 3);
  if (pno < 0) {
    fprintf(stderr, "Animation::color: no such color: %s",
		color);
    return;
  }
  paint(pno);
}

void Animation::change_color(char *clr)
{
	oldPaint_ = paint_;
	color(clr);
}

void Animation::toggle_color() 
{
	int pno = oldPaint_;
	oldPaint_ = paint_;
	paint_ = pno;
}

Animation* Animation::getLastTag()
{
	if (nTag_ > 0) 
		return tags_[0]->getLastTag();
	else 
		return this;
}
