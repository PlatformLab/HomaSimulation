/*
 * Copyright (c) 1997 Regents of the University of California.
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
 *	This product includes software developed by the Daedalus Research
 *	Group at the University of California Berkeley.
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
 * Contributed by Giao Nguyen, http://daedalus.cs.berkeley.edu/~gnguyen
 */

#include "tclcl.h"
#include "tclcl-internal.h"
#include "tracedvar.h"

#ifdef STDC_HEADERS
#include <stdlib.h>  // for abort() on redhat 7.0beta
#endif /* STDC_HEADERS */

#ifdef TCLCL_USE_STREAM
#include <strstream.h>

enum {
	ios__number = ios::basefield + ios::floatfield,
	ios__fields = ios::adjustfield + ios::basefield + ios::floatfield,
};

void TracedVar::format(ostrstream& os)
{
	// format adjustfield and number field only if iosMask is set
	if (iosMask_ & ios::adjustfield)
		os.setf(iosMask_, ios::adjustfield);
	if (iosMask_ & ios__number)
		os.setf(iosMask_, ios__number);
	os.setf(iosMask_, ~ios__fields);
}

void TracedVar::format(ostrstream&)
{
}
#endif

TracedVar::TracedVar() : name_(""), owner_(0), tracer_(0),
#ifdef TCLCL_USE_STREAM
	iosWidth_(0), iosPrecision_(0), iosMask_(0),
#endif /* TCLCL_USE_STREAM */
	next_(0)
{
}

TracedVar::TracedVar(const char* name) : name_(name), owner_(0), tracer_(0),
#ifdef TCLCL_USE_STREAM
	iosWidth_(0), iosPrecision_(0), iosMask_(0),
#endif /* TCLCL_USE_STREAM */
	next_(0)
{
}


void TracedInt::assign(int newval)
{
	// if value changes, set new value and call tracer if exist
	if (val_ == newval)
		return;
	val_ = newval;
	if (tracer_)
		tracer_->trace(this);
}

char* TracedInt::value(char* buf, int buflen)
{
	if (buf == 0)
		return 0;
#ifdef TCLCL_USE_STREAM
	ostrstream ost(buf, 16 + iosWidth_);
	format(ost);
	ost.width(iosWidth_);
	ost << val_;
#else
	// sprintf(buf, "%*d", iosWidth_, val_);
	if (-1 == snprintf(buf, buflen, "%d", val_))
		abort();
#endif
	return buf;
}


void TracedDouble::assign(double newval)
{
	// if value changes, set new value and call tracer if exist
	if (val_ == newval)
		return;
	val_ = newval;
	if (tracer_)
		tracer_->trace(this);
}

char* TracedDouble::value(char* buf, int buflen)
{
	if (buf == 0)
		return 0;
#ifdef TCLCL_USE_STREAM
	ostrstream ost(buf, 16 + iosWidth_);
	format(ost);
	ost.precision(iosPrecision_);
	ost << val_;
#else
	// sprintf(buf, "%*.*f", iosWidth_, iosPrecision_, val_);
	if (-1 == snprintf(buf, buflen, "%g", val_))
		abort();
#endif
	return buf;
}
