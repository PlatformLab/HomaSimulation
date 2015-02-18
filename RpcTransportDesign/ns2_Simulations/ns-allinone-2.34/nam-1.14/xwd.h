
/*
 * xwd.h
 * Copyright (C) 1997 by USC/ISI
 * $Id: xwd.h,v 1.3 2003/10/11 22:56:51 xuanc Exp $
 *
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

#ifndef _xwd_h
#define _xwd_h

extern "C" {

int
xwd_Window_Dump_To_File(
	Tk_Window tk,
	Drawable offscreen,
	unsigned width, unsigned height,
	const char *name);

}
#endif /* _xwd_h */
