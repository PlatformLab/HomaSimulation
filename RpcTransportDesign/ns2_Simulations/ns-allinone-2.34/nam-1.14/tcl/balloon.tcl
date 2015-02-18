#
# Copyright (C) 2000 by USC/ISI
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
# $Header: /cvsroot/nsnam/nam-1/tcl/balloon.tcl,v 1.1 2000/02/18 18:07:31 haoboy Exp $
#
# Generic balloon help for an Animator

Class BalloonHelp

BalloonHelp instproc init { tlw } {
	$self instvar balloon_
	set balloon_ $tlw-bal
        toplevel $balloon_ -class Balloon -borderwidth 1 -relief groove \
		-background black
        label $balloon_.info
        pack $balloon_.info -side left -fill y
        wm overrideredirect $balloon_ 1
        wm withdraw $balloon_
}

BalloonHelp instproc destroy {} {
	$self balloon_cancel
}

# By default show the balloon after the mouse stayed for 500ms.
BalloonHelp instproc balloon_for {win mesg {delay 500}} {
	$self instvar balloon_ bhInfo_ balloon_delay_
	# If we do not know about this window, do the event binding,
	# otherwise just change the message and delay.
	if {![info exists bhInfo_($win)]} {
		set bhInfo_($win) $mesg
		set balloon_delay_ $delay
		bind $win <Enter> "$self balloon_pending %W"
		bind $win <Leave> "$self balloon_cancel"
	} else {
		set bhInfo_($win) $mesg
		set balloon_delay_ $delay
	}
}

BalloonHelp instproc balloon_pending {win} {
	$self instvar bhInfo_ balloon_ balloon_delay_
	$self balloon_cancel
	set bhInfo_(pending) [after $balloon_delay_ \
		"$self balloon_show $win"]
}

BalloonHelp instproc balloon_cancel {} {
	$self instvar bhInfo_ balloon_
	if [info exists bhInfo_(pending)] {
		after cancel $bhInfo_(pending)
		unset bhInfo_(pending)
	}
	wm withdraw $balloon_
}

BalloonHelp instproc balloon_show {win} {
	$self instvar bhInfo_ balloon_

	set bhInfo_(active) 1
	if { $bhInfo_(active) } {
		$balloon_.info configure -text $bhInfo_($win)
		set x [expr [winfo rootx $win]+[winfo width $win]+5]
		set y [expr [winfo rooty $win]+10]
		wm geometry $balloon_ +$x+$y
		wm deiconify $balloon_
		raise $balloon_
	}
	unset bhInfo_(pending)
}

