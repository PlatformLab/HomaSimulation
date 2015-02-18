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
 *       This product includes software developed by the Computer Systems
 *       Engineering Group at Lawrence Berkeley Laboratory.
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
 * ------------
 *
 * Filename: tkcompat.h	
 *   -- created on Thu Oct 02 1997 
 *  
 *  @(#) $Header: /cvsroot/nsnam/nam-1/tkcompat.h,v 1.2 2003/10/11 22:56:51 xuanc Exp $
 *
 * Contents: Tk4.2 / 8.0 compatibility declarations
 */

#ifndef MASH_TKCOMPAT_H
#define MASH_TKCOMPAT_H

#ifndef TK_MAJOR_VERSION
#error "you need to include tk.h first"
#endif

/*
 * Tk4.2 / 8.0 compatibility code
 *
 *  Note that they are not fool-proof implementations, but just enough
 *  to get by
 */
#if TK_MAJOR_VERSION < 8

#include <X11/Xlib.h>

#ifdef __cplusplus
extern "C" {
#endif
	
typedef XFontStruct* Tk_Font;
#define Tk_FontId(pXFontStruct) (pXFontStruct->fid)
#define Tk_TextWidth(pf, str, len) XTextWidth(pf, str, len)

/*
 * The following structure is used by Tk_GetFontMetrics() to return
 * information about the properties of a Tk_Font.  
 */

typedef struct Tk_FontMetrics {
    int ascent;			/* The amount in pixels that the tallest
				 * letter sticks up above the baseline, plus
				 * any extra blank space added by the designer
				 * of the font. */
    int descent;		/* The largest amount in pixels that any
				 * letter sticks below the baseline, plus any
				 * extra blank space added by the designer of
				 * the font. */
    int linespace;		/* The sum of the ascent and descent.  How
				 * far apart two lines of text in the same
				 * font should be placed so that none of the
				 * characters in one line overlap any of the
				 * characters in the other line. */
} Tk_FontMetrics;

extern void Tk_GetFontMetrics(Tk_Font font, Tk_FontMetrics *fmPtr);

#define Tk_DrawChars(display, drawable, gc, tkfont, string, length, x, y) \
	XDrawString(display, drawable, gc, x, y, string, length)

#define Tk_GetFont Tk_GetFontStruct
#define Tk_FreeFont Tk_FreeFontStruct
#define Tk_NameOfFont Tk_NameOfFontStruct		
#define Tk_GetHINSTANCE TkWinGetAppInstance

#ifdef __cplusplus
		};
#endif

		
#endif /* TK_MAJOR_VERSION < 8 */

/* compatible char definition for versions < 8.4 */
/* NOTE: tcl8.3.2 defines CONST, but used it in other places...? */
#if TCL_MAJOR_VERSION < 8 || TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION < 4
  #define CONST84
  #define CONST84_RETURN
#endif /* */
    
#endif /* #ifdef TKCOMPAT_H */ 
