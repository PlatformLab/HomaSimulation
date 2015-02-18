#  Copyright (c) 1997 by the University of Southern California
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
#
#
# The copyright of this module includes the following
# linking-with-specific-other-licenses addition:
#
# In addition, as a special exception, the copyright holders of
# this module give you permission to combine (via static or
# dynamic linking) this module with free software programs or
# libraries that are released under the GNU LGPL and with code
# included in the standard release of ns-2 under the Apache 2.0
# license or under otherwise-compatible licenses with advertising
# requirements (or modified versions of such code, with unchanged
# license).  You may copy and distribute such a system following the
# terms of the GNU GPL for this module and the licenses of the
# other code concerned, provided that you include the source code of
# that other code when and as the GNU GPL requires distribution of
# source code.
#
# Note that people who make modified versions of this module
# are not obligated to grant this special exception for their
# modified versions; it is their choice whether to do so.  The GNU
# General Public License gives permission to release a modified
# version without this exception; this exception also makes it
# possible to release a modified version which carries forward this
# exception.
# 
#  $Header: /cvsroot/nsnam/nam-1/bin/gen-vcmake.pl,v 1.6 2007/02/12 07:08:44 tom_henderson Exp $
#
# This is not to be used as an executable. Rather, it's intended to be invoked
# from Makfefile to generate a makefile.vc

while (<>) {
    (/^\$\(GEN_DIR\)nam_tcl\.cc/ || /^\$\(GEN_DIR\)version.c/) && do {
	# print current line followed by a '-mkdir gen...'
	print $_;
	print "\t-mkdir \$(GEN_DIR:\\\\=)\n";
	next;
    };

    /^makefile\.vc:/ && do {
	# skip this line and the next two lines;
	<>; <>;
	next;
    };

    s/xwd\.o/win32.o getopt.o/o;
    s/^# (\!include)/\!include/o;
    s/\$\(\.SUFFIXES\)//o;
    s/OBJ\) Makefile/OBJ\) makefile.vc/o;
    print $_;
}

exit 0;

