/*
 * Copyright (c) 1993 Regents of the University of California.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and the Network Research Group at
 *      Lawrence Berkeley Laboratory.
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
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/otcl-tclcl/tclcl/Tcl2.cc,v 1.11 2005/09/01 01:54:05 tom_henderson Exp $ (LBL)";
#endif

#include <stdarg.h>
#include "tclcl.h"
#include "tclcl-internal.h"

#ifndef HAVE_SNPRINTF
/*
 * This implementation maps overflow into a hard failure.
 */
int
snprintf(char *buf, int size, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

#ifdef NEED_SUNOS_PROTOS
	/* 
	 * XXX SunOS4's vsprintf returns a char*!!!
	 */
	char *n;
	n = vsprintf(buf, fmt, ap);
	if (strlen(n) >= size)
		abort();
	return strlen(n);
#else
	int n;
	n = vsprintf(buf, fmt, ap);
	if (n >= size)
		abort();
	return n;
#endif
}
#endif /* HAVE_SNPRINTF */

/*
 * Put this routine in a separate module so we can compile
 * it without optimzation to get around a bug in DEC OSF's C++
 * compiler.
 */
void Tcl::evalf(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsprintf(bp_, fmt, ap);
	eval();
}

void Tcl::resultf(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsprintf(bp_, fmt, ap);
	tcl_->result = bp_;
}

void Tcl::add_errorf(const char* fmt, ...)
{
	static char buffer[1024]; /* XXX: individual error lines should not be
				   * larger than 1023 */
	va_list ap;
	va_start(ap, fmt);
	vsprintf(buffer, fmt, ap);
	add_error(buffer);
}

