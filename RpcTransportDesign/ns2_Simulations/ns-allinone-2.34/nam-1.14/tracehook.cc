/*
 * Copyright (c) 1997 University of Southern California.
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
 *      This product includes software developed by the Information Sciences
 *      Institute of the University of Southern California.
 * 4. Neither the name of the University nor of the Institute may be used
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
 */

#include "tclcl.h"
#include "trace.h"
#include "tracehook.h"


/*XXX */

class TraceHookClass : public TclClass {
public:
	TraceHookClass() : TclClass("TraceHook") {}
	TclObject* create(int argc, const char*const* argv) {
		if (argc != 5) 
			return (0);
		return (new TraceHook(argv[4]));
	}
} tracehook_class;


TraceHook::TraceHook(const char *animator) : TraceHandler(animator)
{
}

TraceHook::~TraceHook()
{
}

void TraceHook::update(double /*now*/)
{
}

void TraceHook::reset(double /*now*/)
{
}


//---------------------------------------------------------------------
// void
// TraceHook::handle(const TraceEvent& e, double /*now*/, int /*direction*/) {
//   - Trace event handler for tcl expressions
//   - Calls tracehooktcl in file stats.tcl
//     tracehooktcl is part of the Animator class
//---------------------------------------------------------------------
void TraceHook::handle(const TraceEvent& e, double /*now*/, int /*direction*/) {
    Tcl& tcl = Tcl::instance();

    // debug
    //fprintf(stderr, "%s tracehooktcl {%s}\n", nam()->name(), e.image);
    
    tcl.evalf("%s tracehooktcl {%s}", nam()->name(), e.image);
}

/* interface to Tcl */

int TraceHook::command(int argc, const char *const *argv)
{
	return (TraceHandler::command(argc, argv));
}
