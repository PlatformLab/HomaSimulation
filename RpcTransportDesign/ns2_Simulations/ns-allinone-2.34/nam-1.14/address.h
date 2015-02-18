/*
 * Copyright (c) 1993-1997 Regents of the University of California.
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
 * $Header: /cvsroot/nsnam/nam-1/address.h,v 1.1 1998/04/20 23:49:38 haoboy Exp $
 */

#ifndef ns_addr_params
#define ns_addr_params


#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include <assert.h>



class Address : public TclObject {
 public:
        static Address& instance() { return (*instance_); }
        Address();
	~Address();
	char *print_nodeaddr(int address);
	char *print_portaddr(int address);
	int str2addr(char *str);
	inline int set_word_field(int word, int field, int shift, int mask) {
		return (((field & mask)<<shift) | ((~(mask << shift)) & word));
	}
	int PortShift_;
	int PortMask_;
	/* for now maximum number of hierarchical levels considered as 10 */
	int NodeShift_[10];
	int NodeMask_[10];
	int McastShift_;
	int McastMask_;
	int levels_;
 protected:
	int command(int argc, const char*const* argv);
	static Address* instance_;
	
};

#endif /* ns_addr_params */
