
/*
 * Stolen from X11R6's xwd.c
 * $XConsortium: xwd.c /main/64 1996/01/14 16:53:13 kaleb $
 * by John Heidemann, 15-Dec-97.
 */

/*

Copyright (c) 1987  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.

*/

/*
 * xwd.c MIT Project Athena, X Window system window raster image dumper.
 *
 * This program will dump a raster image of the contents of a window into a 
 * file for output on graphics printers or for other uses.
 *
 *  Author:	Tony Della Fera, DEC
 *		17-Jun-85
 * 
 *  Modification history:
 *
 *  11/14/86 Bill Wyatt, Smithsonian Astrophysical Observatory
 *    - Removed Z format option, changing it to an XY option. Monochrome 
 *      windows will always dump in XY format. Color windows will dump
 *      in Z format by default, but can be dumped in XY format with the
 *      -xy option.
 *
 *  11/18/86 Bill Wyatt
 *    - VERSION 6 is same as version 5 for monchrome. For colors, the 
 *      appropriate number of Color structs are dumped after the header,
 *      which has the number of colors (=0 for monochrome) in place of the
 *      V5 padding at the end. Up to 16-bit displays are supported. I
 *      don't yet know how 24- to 32-bit displays will be handled under
 *      the Version 11 protocol.
 *
 *  6/15/87 David Krikorian, MIT Project Athena
 *    - VERSION 7 runs under the X Version 11 servers, while the previous
 *      versions of xwd were are for X Version 10.  This version is based
 *      on xwd version 6, and should eventually have the same color
 *      abilities. (Xwd V7 has yet to be tested on a color machine, so
 *      all color-related code is commented out until color support
 *      becomes practical.)
 */

/*%
 *%    This is the format for commenting out color-related code until
 *%  color can be supported.
%*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <tk.h>
#include <X11/Xos.h>

#ifdef X_NOT_STDC_ENV
extern int errno;
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <X11/Xmu/WinUtil.h>
typedef unsigned long Pixel;

/* Use our own... */
#include "XWDFile.h"
/*
 * work-around solaris 2.6 problem:
 * SIZEOF is defined in Xmd.h, which is included by XWDFile.h
 * make sure we get the right (ansi) definition).
 */
#if defined(sun) && defined(__svr4__)
#undef SIZEOF
#define SIZEOF(x) sz_##x
#endif


/* work-around solaris 2.6 problem */
#ifndef SIZEOF
#define SIZEOF(x) sz_##x
#endif



/*
 * Window_Dump: dump a window to a file which must already be open for
 *              writting.
 */


static void
outl(char *s)
{
}


/*
 * Determine the pixmap size.
 */

static int
Image_Size(image)
	XImage *image;
{
	if (image->format != ZPixmap)
		return(image->bytes_per_line * image->height * image->depth);

	return(image->bytes_per_line * image->height);
}

#define lowbit(x) ((x) & (~(x) + 1))

static int
ReadColors(tk,cmap,colors)
	Colormap cmap ;
	XColor **colors ;
{
	Visual *vis = Tk_Visual(tk);
	int i,ncolors ;

	ncolors = vis->map_entries;

	if (!(*colors = (XColor *) Tcl_Alloc (sizeof(XColor) * ncolors))) {
		outl("Out of memory!");
		exit(1);
	}

	if (vis->class == DirectColor ||
	    vis->class == TrueColor) {
		Pixel red, green, blue, red1, green1, blue1;

		red = green = blue = 0;
		red1 = lowbit(vis->red_mask);
		green1 = lowbit(vis->green_mask);
		blue1 = lowbit(vis->blue_mask);
		for (i=0; i<ncolors; i++) {
			(*colors)[i].pixel = red|green|blue;
			(*colors)[i].pad = 0;
			red += red1;
			if (red > vis->red_mask)
				red = 0;
			green += green1;
			if (green > vis->green_mask)
				green = 0;
			blue += blue1;
			if (blue > vis->blue_mask)
				blue = 0;
		}
	} else {
		for (i=0; i<ncolors; i++) {
			(*colors)[i].pixel = i;
			(*colors)[i].pad = 0;
		}
	}

	XQueryColors(Tk_Display(tk), cmap, *colors, ncolors);
    
	return(ncolors);
}

/*
 * Get the XColors of all pixels in image - returns # of colors
 */
static int
Get_XColors(tk, colors)
	Tk_Window tk;
	XColor **colors;
{
	int ncolors;
	Colormap cmap = Tk_Colormap(tk);

#if 0
	if (use_installed)
		/* assume the visual will be OK ... */
		cmap = XListInstalledColormaps(dpy, win_info->root, &i)[0];
#endif
	if (!cmap)
		return(0);
	ncolors = ReadColors(tk,cmap,colors) ;
	return ncolors ;
}

static void
_swapshort (bp, n)
	register char *bp;
	register unsigned n;
{
	register char c;
	register char *ep = bp + n;

	while (bp < ep) {
		c = *bp;
		*bp = *(bp + 1);
		bp++;
		*bp++ = c;
	}
}

static void
_swaplong (bp, n)
	register char *bp;
	register unsigned n;
{
	register char c;
	register char *ep = bp + n;
	register char *sp;

	while (bp < ep) {
		sp = bp + 3;
		c = *sp;
		*sp = *bp;
		*bp++ = c;
		sp = bp + 1;
		c = *sp;
		*sp = *bp;
		*bp++ = c;
		bp += 2;
	}
}

void
xwd_Window_Dump(tk, offscreen, width, height, out)
	Tk_Window tk;
	Drawable offscreen;
	unsigned width, height;
	FILE *out;
{
	unsigned long swaptest = 1;
	XColor *colors;
	unsigned buffer_size;
	int header_size;
	int ncolors, i;
	char *win_name = "a";
	int win_name_size = 1; /* strlen("a") */
	XImage *image;
	int absx, absy, x, y;
	XWDFileHeader header;
	XWDColor xwdcolor;
	int format = ZPixmap;

	int debug = 0;
	Display *dpy = Tk_Display(tk);
	Visual *vis;

	absx = absy = 0;

	image = XGetImage (dpy, offscreen, 0, 0,
			   width, height, AllPlanes, format);

	if (!image) {
		fprintf (stderr, "%s:  unable to get image at %dx%d+%d+%d\n",
			 "xwd", width, height, x, y);
		exit (1);
	}

	/*
	 * Determine the pixmap size.
	 */
	buffer_size = Image_Size(image);

	if (debug) outl("xwd: Getting Colors.\n");

	ncolors = Get_XColors(tk, &colors);
	vis = Tk_Visual(tk);

	/*
	 * Calculate header size.
	 */
	if (debug) outl("xwd: Calculating header size.\n");
	header_size = SIZEOF(XWDheader) + win_name_size;

	/*
	 * Write out header information.
	 */
	if (debug) outl("xwd: Constructing and dumping file header.\n");
	header.header_size = (CARD32) header_size;
	header.file_version = (CARD32) XWD_FILE_VERSION;
	header.pixmap_format = (CARD32) format;
	header.pixmap_depth = (CARD32) image->depth;
	header.pixmap_width = (CARD32) image->width;
	header.pixmap_height = (CARD32) image->height;
	header.xoffset = (CARD32) image->xoffset;
	header.byte_order = (CARD32) image->byte_order;
	header.bitmap_unit = (CARD32) image->bitmap_unit;
	header.bitmap_bit_order = (CARD32) image->bitmap_bit_order;
	header.bitmap_pad = (CARD32) image->bitmap_pad;
	header.bits_per_pixel = (CARD32) image->bits_per_pixel;
	header.bytes_per_line = (CARD32) image->bytes_per_line;
	/****
	  header.visual_class = (CARD32) win_info.visual->class;
	  header.red_mask = (CARD32) win_info.visual->red_mask;
	  header.green_mask = (CARD32) win_info.visual->green_mask;
	  header.blue_mask = (CARD32) win_info.visual->blue_mask;
	  header.bits_per_rgb = (CARD32) win_info.visual->bits_per_rgb;
	  header.colormap_entries = (CARD32) win_info.visual->map_entries;
	  *****/
	header.visual_class = (CARD32) vis->class;
	header.red_mask = (CARD32) vis->red_mask;
	header.green_mask = (CARD32) vis->green_mask;
	header.blue_mask = (CARD32) vis->blue_mask;
	header.bits_per_rgb = (CARD32) vis->bits_per_rgb;
	header.colormap_entries = (CARD32) vis->map_entries;

	header.ncolors = ncolors;
	header.window_width = (CARD32) width;
	header.window_height = (CARD32) height;
	header.window_x = absx;
	header.window_y = absy;
	header.window_bdrwidth = (CARD32) 0;

	if (*(char *) &swaptest) {
		_swaplong((char *) &header, sizeof(header));
		for (i = 0; i < ncolors; i++) {
			_swaplong((char *) &colors[i].pixel, sizeof(long));
			_swapshort((char *) &colors[i].red, 3 * sizeof(short));
		}
	}

	if (fwrite((char *)&header, SIZEOF(XWDheader), 1, out) != 1 ||
	    fwrite(win_name, win_name_size, 1, out) != 1) {
		perror("xwd");
		exit(1);
	}

	/*
	 * Write out the color maps, if any
	 */

/*    if (debug) outl("xwd: Dumping %d colors.\n", ncolors); */
	for (i = 0; i < ncolors; i++) {
		xwdcolor.pixel = colors[i].pixel;
		xwdcolor.red = colors[i].red;
		xwdcolor.green = colors[i].green;
		xwdcolor.blue = colors[i].blue;
		xwdcolor.flags = colors[i].flags;
		if (fwrite((char *) &xwdcolor, SIZEOF(XWDColor), 1, out) != 1) {
			perror("xwd");
			exit(1);
		}
	}

	/*
	 * Write out the buffer.
	 */
/*    if (debug) outl("xwd: Dumping pixmap.  bufsize=%d\n",buffer_size); */

	/*
	 *    This copying of the bit stream (data) to a file is to be replaced
	 *  by an Xlib call which hasn't been written yet.  It is not clear
	 *  what other functions of xwd will be taken over by this (as yet)
	 *  non-existant X function.
	 */
	if (fwrite(image->data, (int) buffer_size, 1, out) != 1) {
		perror("xwd");
		exit(1);
	}

	/*
	 * free the color buffer.
	 */

	if(debug && ncolors > 0) outl("xwd: Freeing colors.\n");
	if(ncolors > 0) Tcl_Free((char*)colors);

	/*
	 * Free image
	 */
	XDestroyImage(image);
}

int
xwd_Window_Dump_To_File(
	Tk_Window tk,
	Drawable offscreen,
	unsigned width, unsigned height,
	char *name)
{
	FILE *f;

	if (!(f = fopen(name, "w"))) {
		fprintf(stderr, "cannot open %s", name);
		return TCL_ERROR;
	};
	xwd_Window_Dump(tk, offscreen, width, height, f);
	fclose(f);
	return TCL_OK;
}

