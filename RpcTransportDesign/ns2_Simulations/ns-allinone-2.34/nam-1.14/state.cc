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
 * @(#) $Header: /cvsroot/nsnam/nam-1/state.cc,v 1.1.1.1 1997/06/16 22:40:29 mjh Exp $ (LBL)
 */

#include "state.h"

State* State::instance_;

void State::init(int s)
{
	instance_ = new State(s);
}

State::State(int s) : cnt_(0), size_(s)
{
	states = new StateItem[size_];
}

/* Set the times at which state info should be saved. */
void State::setTimes(double mintime, double maxtime)
{
	double incr = (maxtime - mintime) / size_;
	double tim = mintime;
	for (int i = 0; i < size_; i++) {
		tim += incr;
		states[i].time = tim;
	}
}

void State::get(double tim, StateInfo& s)
{
	for (int i = cnt_-1; i >= 0; i--)
		if (states[i].time < tim) {
			s = states[i].si;
			return;
		}
	s.time = 0.;
	s.offset = 0;
}

void State::set(double tim, StateInfo& info)
{
	if (cnt_ < size_) {
		states[cnt_].time = tim;
		states[cnt_++].si = info;
	}
}

/* Return the next time that state info should be saved. */
double State::next()
{
	return (states[cnt_].time);
}
