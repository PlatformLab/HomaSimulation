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
# $Header: /cvsroot/nsnam/nam-1/tcl/annotation.tcl,v 1.4 1999/01/11 18:35:39 haoboy Exp $

# popup an annotation command menu
Animator instproc popup_annotation { w x y } {
	$self instvar annoBox running 
	$self stop 1
	catch {destroy $w.menuPopa}
	menu $w.menuPopa -relief groove -borderwidth 2
	$w.menuPopa add command -label "Command"
	$w.menuPopa add separator
	$w.menuPopa add command -label "Add" \
		-command "$self cmdAnnotationAdd $w $x $y"
	set index [$annoBox nearest $y]
	$w.menuPopa add command -label "Delete" \
		-command "$self cmdAnnotationDelete $index"
	$w.menuPopa add command -label "Info" \
		-command "$self cmdAnnotationInfo $w $index $x $y"
	set rx [winfo rootx $w]
	set ry [winfo rooty $w]
	set rx [expr $rx + $x]
	set ry [expr $ry + $y]
	tk_popup $w.menuPopa $rx $ry
}

# Insert a annotation *after* the annotation pointed to by the current click
# Will prompt for an input string
Animator instproc cmdAnnotationAdd { w x y } {
	$self instvar annoInputText now

	# Popup a frame to get input string
	# 
	catch { destroy .inputBox }
	set f [smallfont]
	toplevel .inputBox
	label .inputBox.title -text "New Annotation Text" -font $f
	set annoInputText ""
	entry .inputBox.input -width 40 -relief sunken \
		-textvariable annoInputText
	button .inputBox.done -text "Done" -borderwidth 2 -relief raised \
		-font $f \
		-command "add_annotation $now; catch {destroy .inputBox}"
	pack .inputBox.title -side top
	pack .inputBox.input -side top
	pack .inputBox.done -side top

	bind .inputBox.input <Return> \
		"add_annotation $now; catch { destroy .inputBox }"
}

Animator instproc cmdAnnotationDelete { index } {
	$self instvar annoBox annoTimeList
	$annoBox delete $index
	set annoTimeList [lreplace $annoTimeList $index $index]
}

Animator instproc cmdAnnotationInfo { w index x y } {
	$self instvar annoBox annoTimeList
	set str [$annoBox get $index]
	set lst [list "At time" [lindex $annoTimeList $index] ", $str"]
	set str [join $lst]

	# Since tk_messageBox is only available after 8.0, we make it
	# on our own.
	catch { destroy $w.mBox }
	set f [smallfont]
	frame $w.mBox -relief groove -borderwidth 2
	label $w.mBox.message -text $str -font $f
	button $w.mBox.done -text "Done" -borderwidth 2 -relief raised \
		-font $f \
		-command "catch { destroy $w.mBox }"
	pack $w.mBox.message
	pack $w.mBox.done
	place_frame $w $w.mBox $x $y
}

Animator instproc add_annotation { time } {
	$self instvar annoBox annoTimeList annoInputText

	#puts "Add annotation \"$annoInputText\" at time $time"
	set size [$annoBox size]
	for {set index 0} {$index < $size} {incr index} {
		if {[lindex $annoTimeList $index] > $time} {
			break
		}
	}

	if {$index == $size} {
		set index "end"
	}
	$annoBox insert $index "$annoInputText"
	if {$size == 0} {
		lappend annoTimeList $time
	} else {
		set annoTimeList [linsert $annoTimeList $index $time]
	}
	$annoBox see $index
}

# Can't use 'now'. Observed when sim_annotation gets executed, 'now' may not 
# be exactly the desired time.
Animator instproc sim_annotation { time seq args } {
	$self instvar annoBox annoTimeList annoBoxHeight regAnno

	# Only add annotation once and never delete them
	if ![info exists regAnno($seq)] {
		# insert when going forward
		$annoBox insert end "$args"
		set size [$annoBox size]
		set annoTimeList [lappend annoTimeList $time]
		set regAnno($seq) 1
		set d [expr $size-$annoBoxHeight]
		if {$d > 0} {
			$annoBox yview $d
		}
		# puts "At time $time, added $args"
	}
}

# Set center of annotation box to the current time
Animator instproc update_annotations { time } {
	$self instvar annoBox annoTimeList annoBoxHeight direction annoJump

	set size [$annoBox size]
	if {($size == 0) || ($annoJump == 1)} {
		set annoJump 0
		return 
	}
	set i 0
	foreach l $annoTimeList {
		if {$l > $time} {
			break
		} else {
			incr i
		}
	}
	if {$direction != 1 && $i != 0} {
		incr i -1
	}
	if {$i < $annoBoxHeight/2} {
		set d 0
	} else {
		set d [expr $i - $annoBoxHeight / 2]
	}
	$annoBox yview $d
	$annoBox selection clear 0 $size
	$annoBox selection set $i $i
}

Animator instproc jump_to_annotation { anno } {
	$self instvar annoTimeList annoJump
	set idx [$anno index active]
	#puts "Jumping to time [lindex $annoTimeList $idx]"
	set annoJump 1
	$self settime [lindex $annoTimeList $idx]
	$self peer_cmd 1 "settime [lindex $annoTimeList $idx]"
}

#
# The following functions are actually the functions used in the 'v' events 
# in the nam traces
# The 'v' events are generated by dynamic links (rtmodel). Whenever you have 
# a nam trace which has dynamic link traces, you'll need to define the 
# following functions.
#
Animator instproc link-up {now src dst} {
	$self sim_annotation $now link($src:$dst) up
}

Animator instproc link-down {now src dst} {
	$self sim_annotation $now link($src:$dst) down
}

Animator instproc node-up {now src} {
	$self sim_annotation $now $node $src up
}

Animator instproc node-down {now src} {
	$self sim_annotation $now $node $src down
}
