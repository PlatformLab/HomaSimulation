/* autoconf.h.  Generated automatically by configure.  */
/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1997 University of Southern California.
 * All rights reserved.                                            
 *                                                                
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation, advertising
 * materials, and other materials related to such distribution and use
 * acknowledge that the software was developed by the University of
 * Southern California, Information Sciences Institute.  The name of the
 * University may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/* This file should contain variables changed only by autoconf. */

/* XXX Change the following two variables to where your perl and tcl are located! */
#define NSPERL_PATH "C:\\Program\\Perl\\bin\\perl.exe"
#define NSTCLSH_PATH "C:\\Program\\Tcl\\bin\\tclsh80.exe"

/* If you need these from tcl, see the file tcl/lib/ns-autoconf.tcl.in */

/*
 * Put autoconf #define's here to keep them off the command line.
 * see autoconf.info(Header Templates) in the autoconf docs.
 */

/* what does random(3) return? */
#define RANDOM_RETURN_TYPE int

/* type definitions */
typedef char int8_t;		/* cygwin-b20\H-i586-cygwin32\i586-cygwin32\include\sys\types.h(83) */
typedef unsigned char u_int8_t; /* cygwin-b20\H-i586-cygwin32\i586-cygwin32\include\sys\types.h(84) */
typedef short int16_t;		/* cygnus\cygwin-b20\H-i586-cygwin32\i586-cygwin32\include\sys\types.h(85) */
typedef unsigned short u_int16_t; /* from cygwin-b20\H-i586-cygwin32\i586-cygwin32\include\sys\types.h(86) */
typedef int int32_t;		/* cygwin-b20\H-i586-cygwin32\i586-cygwin32\include\sys\types.h(87) */
typedef unsigned int u_int32_t; /* cygwin-b20\H-i586-cygwin32\i586-cygwin32\include\sys\types.h(88) */

typedef __int64 int64_t;	/* C:\Program\Microsoft Visual Studio\VC98\CRT\SRC\STRTOQ.C(19) */
typedef unsigned __int64 u_int64_t; /* C:\Program\Microsoft Visual Studio\VC98\CRT\SRC\STRTOQ.C(20) */
#undef HAVE_INT64
#define SIZEOF_LONG 4

/* functions */
#undef HAVE_BCOPY
#undef HAVE_BZERO
#undef HAVE_GETRUSAGE
#undef HAVE_SBRK
#undef HAVE_SNPRINTF
#undef HAVE_STRTOLL
#undef HAVE_STRTOQ

/* headers */
#define HAVE_STRING_H 1
#undef HAVE_STRINGS_H
#undef HAVE_ARPA_INET_H
#undef HAVE_NETINET_IN_H

