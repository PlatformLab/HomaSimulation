/*
 * Copyright (c) 1990-1997 Regents of the University of California.
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
 * $Header: /cvsroot/nsnam/nam-1/address.cc,v 1.2 1998/04/24 21:09:12 haoboy Exp $
 */


#include <stdio.h>
#include <stdlib.h>
#include "tclcl.h"
#include "address.h"
#include "config.h"



static class AddressClass : public TclClass {
public:
	AddressClass() : TclClass("Address") {} 
	TclObject* create(int, const char*const*) {
		return (new Address());
	}
} class_address;



Address* Address::instance_;


Address::Address() : PortShift_(0), PortMask_(0), McastShift_(0), McastMask_(0), levels_(0)
{
  for (int i = 0; i < 10; i++) {
    NodeShift_[i] = 0;
    NodeMask_[i] = 0;
  }
  // setting instance_ should be in constructor, instead of Address::
  if ((instance_ == 0) || (instance_ != this))
	  instance_ = this;
}


Address::~Address() { }

int Address::command(int argc, const char*const* argv)
{
    int i, c, temp=0;
    
    Tcl& tcl = Tcl::instance();
    //    if ((instance_ == 0) || (instance_ != this))
    //      instance_ = this;
    if (argc == 4) {
	if (strcmp(argv[1], "portbits-are") == 0) {
	    PortShift_ = atoi(argv[2]);
	    PortMask_ = atoi(argv[3]);
	    return (TCL_OK);
	}

	if (strcmp(argv[1], "mcastbits-are") == 0) {
	    McastShift_ = atoi(argv[2]);
	    McastMask_ = atoi(argv[3]);
	    return (TCL_OK);
	}
    }
    if (argc >= 4) {
	    if (strcmp(argv[1], "add-hier") == 0) {
		    /*
		     * <address> add-hier <level> <mask> <shift>
		     */
		    int level = atoi(argv[2]);
		    int mask = atoi(argv[3]);
		    int shift = atoi(argv[4]);
		    if (levels_ < level)
			    levels_ = level;
		    NodeShift_[level] = shift;
		    NodeMask_[level] = mask;
		    return (TCL_OK);
	    }

	if (strcmp(argv[1], "idsbits-are") == 0) {
	    temp = (argc - 2)/2;
	    if (levels_) { 
		if (temp != levels_) {
		    tcl.resultf("#idshiftbits don't match with #hier levels\n");
		    return (TCL_ERROR);
		}
	    }
	    else 
		levels_ = temp;
	    // NodeShift_ = new int[levels_];
	    for (i = 3, c = 1; c <= levels_; c++, i+=2) 
		NodeShift_[c] = atoi(argv[i]);
	    return (TCL_OK); 
	}
	
	if (strcmp(argv[1], "idmbits-are") == 0) {
	    temp = (argc - 2)/2;
	    if (levels_) { 
		if (temp != levels_) {
		    tcl.resultf("#idmaskbits don't match with #hier levels\n");
		    return (TCL_ERROR);
		}
	    }
	    else 
		levels_ = temp;
	    // NodeMask_ = new int[levels_];
	    for (i = 3, c = 1; c <= levels_; c++, i+=2) 
		NodeMask_[c] = atoi(argv[i]);
	    return (TCL_OK);
	}
    }
    return TclObject::command(argc, argv);
}

char *Address::print_nodeaddr(int address)
{
  int a;
  char temp[SMALL_LEN];
  char str[SMALL_LEN];
  char *addrstr;
  
  str[0] = '\0';
  for (int i=1; i <= levels_; i++) {
      a = address >> NodeShift_[i];
      if (levels_ > 1)
	  a = a & NodeMask_[i];
      sprintf(temp, "%d.", a);
      strcat(str, temp);
  }
  addrstr = new char[strlen(str)];
  strcpy(addrstr, str);
//   printf("Nodeaddr - %s\n",addrstr);
  return(addrstr);
}


char *Address::print_portaddr(int address)
{
  int a;
  char str[SMALL_LEN];
  char *addrstr;
  
  str[0] = '\0';
  a = address >> PortShift_;
  a = a & PortMask_;
  sprintf(str, "%d", a);
  addrstr = new char[strlen(str)];
  strcpy(addrstr, str);
//   printf("Portaddr - %s\n",addrstr);
  return(addrstr);
}

// Convert address in string format to binary format (int). 
int Address::str2addr(char *str)
{
	if (levels_ == 0) 
		return atoi(str);
	char *delim = ".";
	char *tok;
	int addr;
	for (int i = 1; i <= levels_; i++) {
		if (i == 1) {
			tok = strtok(str, delim);
			addr = atoi(tok);
		} else {
			tok = strtok(NULL, delim);
			addr = set_word_field(addr, atoi(tok), 
					      NodeShift_[i], NodeMask_[i]);
		}
	}
	return addr;
}
