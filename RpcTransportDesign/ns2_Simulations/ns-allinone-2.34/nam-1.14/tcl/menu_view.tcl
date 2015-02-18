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
# $Header: /cvsroot/nsnam/nam-1/tcl/menu_view.tcl,v 1.10 2001/07/30 22:16:46 mehringe Exp $

Animator instproc new_view {} {
    $self instvar netModel viewctr nam_name netViews tlw_

    toplevel $tlw_.v$viewctr
    set w $tlw_.v$viewctr

    incr viewctr
    wm title $w $nam_name
    frame $w.f
    #frame is just to sink the newView
    frame $w.f.f -borderwidth 2 -relief sunken
    # Do not use editview!!
    $netModel view $w.f.f.net
    set newView $w.f.f.net
    lappend netViews $newView
    pack $w.f.f.net -side top -expand true -fill both
    
    $newView xscroll $w.f.hsb
    scrollbar $w.f.hsb -orient horizontal -width 10 -borderwidth 1 \
	    -command "$newView xview"
    $w.f.hsb set 0.0 1.0
    pack $w.f.hsb -side bottom -fill x
    pack $w.f.f -side top -fill both -expand true
    
    frame $w.f2
    $newView yscroll $w.f2.vsb
    scrollbar $w.f2.vsb -orient vertical -width 10 -borderwidth 1 \
	    -command "$newView yview"
    $w.f2.vsb set 0.0 1.0
    pack $w.f2.vsb -side top -fill y -expand true
    frame $w.f2.l -width 12 -height 12
    pack $w.f2.l -side top

    frame $w.ctrl -borderwidth 2 -relief groove
    $self build-zoombar $newView $w.ctrl $w
    pack $w.ctrl -side left -fill y

    pack $w.f2 -side right -fill y
    pack $w.f -side left -fill both -expand true
    $self window_bind $w
    $self view_bind $newView
}

Animator instproc energy_view {} {
  $self instvar netView now vslider windows nam_name graphs tlw_ tracefile

  set name "node_energy"

  set graphtrace [new Trace $tracefile $self]
  set netgraph [new EnergyNetworkGraph]
  $windows(title) configure -text "Please wait - reading tracefile..."
  update
  set maxtime [$graphtrace maxtime]
  set mintime [$graphtrace mintime]
  $graphtrace connect $netgraph
  $netgraph timerange $mintime $maxtime

  #force the entire tracefile to be read
  $graphtrace settime $maxtime 1

  set w $tlw_.graph
  if {[winfo exists $w]==0} {
       frame $w
       pack $w -side top -fill x -expand true -after [$vslider frame]
  }

  lappend graphs $netgraph
  frame $w.f$name -borderwidth 2 -relief groove
  pack $w.f$name -side top -expand true -fill both
  label $w.f$name.pr -bitmap pullright -borderwidth 1 -relief raised
  bind $w.f$name.pr <Enter> \
        "$self viewgraph_label \"node of no energy left\" \
         $w.f$name $w.f$name.pr $netgraph"
  pack $w.f$name.pr -side left
  $netgraph view $w.f$name.view

  #set the current time in the graph
  $netgraph settime $now

  pack $w.f$name.view -side left -expand true \
	               -fill both
  frame $w.f$name.l2 -width [expr [$vslider swidth]/2] -height 30
  pack $w.f$name.l2 -side left
  $windows(title) configure -text $nam_name

}


Animator instproc view_drag_start {view x y} {
	$self instvar drag
	set drag($view,x) $x
	set drag($view,y) $y
}

Animator instproc view_drag_motion {view x y} {
	$self instvar drag
        set MAX_DRAG_LENGTH 0.008

	set dx [expr ($drag($view,x) - $x)]
	set dy [expr ($drag($view,y) - $y)]

        if {$dx > $MAX_DRAG_LENGTH} {
          set dx $MAX_DRAG_LENGTH
        }

        if {$dx < -$MAX_DRAG_LENGTH} {
          set dx -$MAX_DRAG_LENGTH
        }

        if {$dy > $MAX_DRAG_LENGTH} {
          set dy $MAX_DRAG_LENGTH
        }

        if {$dy < -$MAX_DRAG_LENGTH} {
          set dy -$MAX_DRAG_LENGTH
        }
        
	$view xview scroll $dx world
	$view yview scroll $dy world
	$self view_drag_start $view $x $y
}

# Creation of an EditView. Currently only one editview is allowed.
Animator instproc new_editview {} {
	$self instvar netModel nam_name NETWORK_MODEL tlw_

	#if {$NETWORK_MODEL == "NetworkModel"} {
	#	tk_messageBox -title "Warning" -message \
	#		"Editing works only with auto layout." \
	#		-type ok
	#	return
	#}
	if [winfo exists $tlw_.editview] { 
		return 
	}
	toplevel $tlw_.editview
	set w $tlw_.editview

	wm title $w $nam_name
	frame $w.f
	#frame is just to sink the newView
	frame $w.f.f -borderwidth 2 -relief sunken
	$netModel editview $w.f.f.edit
	set newView $w.f.f.edit
	pack $w.f.f.edit -side top -expand true -fill both

	$newView xscroll $w.f.hsb
	scrollbar $w.f.hsb -orient horizontal -width 10 -borderwidth 1 \
		    -command "$newView xview"
	$w.f.hsb set 0.0 1.0
	pack $w.f.hsb -side bottom -fill x
	pack $w.f.f -side top -fill both -expand true
    
	frame $w.f2
	$newView yscroll $w.f2.vsb
	scrollbar $w.f2.vsb -orient vertical -width 10 -borderwidth 1 \
			-command "$newView yview"
	$w.f2.vsb set 0.0 1.0
	pack $w.f2.vsb -side top -fill y -expand true
	frame $w.f2.l -width 12 -height 12
	pack $w.f2.l -side top

	# Here we are going to put control buttons, but not now
	frame $w.ctrl -borderwidth 2 -relief groove
	$self build-zoombar $newView $w.ctrl $w
	pack $w.ctrl -side left -fill y

	pack $w.f2 -side right -fill y
	pack $w.f -side left -fill both -expand true

	$self editview_bind $w.f.f.edit
}

# Interaction in TkView
Animator instproc editview_bind { ev } {
	# If there is some object, then select it; otherwise set current
	# point and prepare to start a rubber band rectangle
	bind $ev <ButtonPress-1> "$ev setPoint \%x \%y 0"

	# Add an object to selection
	bind $ev <Shift-ButtonPress-1> "$ev setPoint \%x \%y 1"

	bind $ev <ButtonRelease-3> "$ev dctag"

	# If any object is selected, set the object's position to point (x,y); 
	# otherwise set the rubber band rectangle and select all the 
	# objects in that rectangle
	# Note: we need a default tag for the selection in rubber band.
	bind $ev <ButtonRelease-1> "$ev releasePoint \%x \%y"

	# If any object(s) are selected, move the object's shadow to the
	# current point; otherwise move the current point and set rubber 
	# band
	bind $ev <Any-B1-Motion> "$ev moveTo \%x \%y"
}

Animator instproc clear_editview_bind { ev } {
	bind $ev <ButtonPress-1> ""
	bind $ev <Shift-ButtonPress-1> ""
	bind $ev <ButtonRelease-3> ""
	bind $ev <ButtonRelease-1> ""
	bind $ev <Any-B1-Motion> ""
	$ev view_mode
}
