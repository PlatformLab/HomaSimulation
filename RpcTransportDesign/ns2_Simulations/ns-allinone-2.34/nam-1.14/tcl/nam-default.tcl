#
# Copyright (C) 1998 by USC/ISI
# All rights reserved.                                            
#                                                                
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation, advertising
# materials, and other materials related to such distribution and use
# acknowledge that the software was developed by the University of
# Southern California, Information Sciences Institute.  The name of the
# University may not be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
# 
# Initialization of constants
#
# $Header: /cvsroot/nsnam/nam-1/tcl/nam-default.tcl,v 1.7 2007/02/18 22:44:54 tom_henderson Exp $

set tk_strictMotif 0

set uscale(m) 1e-3
set uscale(u) 1e-6
set uscale(k) 1e3
set uscale(K) 1e3
set uscale(M) 1e6

# Overwrite this in your .nam.tcl to use derived animator-control class
set ANIMATOR_CONTROL_CLASS AnimControl

# Class variables
Animator set id_ 0

AnimControl set instance_ 	""
AnimControl set INIT_PORT_ 	20000	;# Arbitrary value
AnimControl set PORT_FILE_ 	"[lindex [glob ~] 0]/.nam-port"
	;# File which stores server port

AnimControl set ANIMATOR_CLASS_	Animator
	;# Overwrite this in .nam.tcl to use derived animator class

Animator set welcome "                               Welcome to NamGraph 1.14                         "
