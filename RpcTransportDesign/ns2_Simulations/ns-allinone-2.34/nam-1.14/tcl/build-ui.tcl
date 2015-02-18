
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
# $Header: /cvsroot/nsnam/nam-1/tcl/build-ui.tcl,v 1.32 2002/11/25 20:14:32 buchheim Exp $

Animator instproc build-ui {} {
	$self instvar netView id_ tlw_ balloon_

	# Start our own toplevel widget
	set tlw_ .model$id_
	toplevel $tlw_

	# Create balloon help for this animator
	set balloon_ [new BalloonHelp $tlw_]

        # build the menu bar
        frame $tlw_.menu -borderwidth 0
	$self build-menus $tlw_.menu
        pack $tlw_.menu -side top -fill x

	# Every netmodel has: view (zoom bar, scroll bar, 
	# monitor pane, layout pane, annotation pane, time slider)
	frame $tlw_.view
	$self build-view $tlw_.view

        # build the control panel with rew, ffwd, play, etc
	frame $tlw_.ctrl0 -relief flat -borderwidth 0
	$self build-animation-ctrl $tlw_.ctrl0 
        pack $tlw_.ctrl0 -side top -fill x

	frame $tlw_.ctrl -relief flat -borderwidth 0
	$self build-view-ctrl $tlw_.ctrl
	set windows(control) $tlw_.ctrl
	pack $tlw_.ctrl -side bottom -fill x

	pack $tlw_.view -side top -fill both -expand true

	wm minsize $tlw_ 200 200
        $self window_bind $tlw_
        $self view_bind $netView

	# Stop the animation if the window is destroyed by the window manager
	bind $netView <Destroy> "$self stop 1"

	# Set window title to "nam #instance: trace_file_name"
	$self instvar tracefile
	wm title $tlw_ "[tk appname]: $tracefile"
}

Animator instproc build-view w {
	$self tkvar showpanel
	$self instvar netModel netView netViews tlw_ windows

	# View
        frame $w.f
        #frame is just to sink the netview
        frame $w.f.f -borderwidth 2 -relief sunken
#	$netModel view $w.f.f.net
	$netModel editview $w.f.f.net
        set netView $w.f.f.net
	set netViews $netView
        pack $w.f.f.net -side top -expand true -fill both

	# X scroll bar
        $netView xscroll $w.f.hsb
        scrollbar $w.f.hsb -orient horizontal -width 10 \
		-borderwidth 1 -command "$netView xview"
        $w.f.hsb set 0.0 1.0
        pack $w.f.hsb -side bottom -fill x
        pack $w.f.f -side top -fill both -expand true

	# Y scroll bar
        frame $w.f2
        $netView yscroll $w.f2.vsb
        scrollbar $w.f2.vsb -orient vertical -width 10 \
		-borderwidth 1 -command "$netView yview"
        $w.f2.vsb set 0.0 1.0
        pack $w.f2.vsb -side top -fill y -expand true
        frame $w.f2.l -width 12 -height 12
        pack $w.f2.l -side top

	# zoom bar
	frame $w.ctrl -borderwidth 2 -relief groove
	$self build-zoombar $netView $w.ctrl $w
	pack $w.ctrl -side left -fill y
        pack $w.f2 -side right -fill y
        pack $w.f -side left -fill both -expand true

	# monitor pane
	frame $tlw_.monwin -relief groove -borderwidth 2
	button $tlw_.monwin.l -bitmap monitors -borderwidth 2 -relief groove \
		-command "$self closepanel monitor"
	pack $tlw_.monwin.l -side left -fill y
	set windows(monitor) $tlw_.monwin
	trace variable showpanel(monitor) w \
		"$self displaypanel $tlw_.monwin $w monitor top x false"
	set showpanel(monitor) 0
}

Animator instproc build-view-ctrl {w} {
	$self tkvar showpanel
	$self instvar NETWORK_MODEL mintime maxtime now mslider pipemode \
		vslider

	# Time slider pane
	frame $w.p0b -relief flat -borderwidth 0

#	$self build-slider $w.p0b

	set mslider [new TimesliderModel $mintime $maxtime $now $self]
	set vslider [new TimesliderView $w.p0b $mslider]

	$mslider addObserver $vslider
	$mslider setpipemode $pipemode

	# annotation pane
	frame $w.tl -relief flat -borderwidth 0
	$self build-annotation $w.tl
	set showpanel(annotate) 1

	if {$NETWORK_MODEL == "NetworkModel/Auto"} {
	        set showpanel(autolayout) 1
	        trace variable showpanel(autolayout) w \
		    "$self displaypanel $w.p2 $w.p0b autolayout \
		     top x false"
		frame $w.p2 -relief flat -borderwidth 0
		$self build-layout $w.p2
		pack $w.p0b $w.p2 $w.tl  \
			-side top -fill x
	} else {
	        set showpanel(autolayout) 0
		pack $w.p0b $w.tl -side top -fill x
	}
}

Animator instproc build-menus w {
	$self tkvar nam_record_animation nam_auto_ff showpanel isSync_
	$self tkvar showData_ showRouting_ showMac_
        $self instvar NETWORK_MODEL netModel nam_name windows 

	frame $w.menu -relief groove -bd 2
        pack $w.menu -side top -fill x

	set padx 4

	set mb $w.menu.file
	set m $mb.m
	menubutton $mb -text "File" -menu $m -underline 0 \
		-borderwidth 1 
	menu $m
	#$m add command -label "Open..." -state disabled
	$m add command -label "Save layout" -command "$self save_layout"
	$m add command -label "Print" -command "$self print_view $netModel nam"
	$m add separator
	# NEEDSWORK: record animation should prompt for the
	# prefix to use (nam_record_filename) in a dialog box.
	# Also, the file opening/closing should be moved to Tcl
	# so I can write to a
	# filename "|xwdtoppm | ppmtogif > sequenceXXX.gif"
	# and have the Right Thing happen.
	# Finally, it would be nice to put the clock
	# actually into the image so it makes it into the mpeg.
	$m add checkbutton -label "Record animation" \
		-variable [$self tkvarname nam_record_animation]
	if ![info exists nam_record_animation] {
		set nam_record_animation 0
	}
	$m add checkbutton -label "Auto FastForward" \
		-variable [$self tkvarname nam_auto_ff]
	if ![info exists nam_auto_ff] {
		set nam_auto_ff 0
	}

	$m add separator
	$m add command -label "Close" -command "$self done"
	pack $mb -side left -padx $padx

	set mb $w.menu.views
	set m $mb.m
	set mp $m.filter
	set mp1 $mp.type1
	set mp2 $mp.type2
	menubutton $mb -text "Views" -menu $m -underline 0 \
		-borderwidth 1 
	menu $m
	$m add command -label "New view" -command "$self new_view"
#	$m add command -label "Edit view" -command "$self new_editview"
	$m add separator

	$m add command -label "Node energy" -command "$self energy_view"

	#filter menu
	$m add cascade -label "Packet filter" -menu $mp
	menu $mp
 
	$mp add cascade -label "Packet type" -menu $mp1
	$mp add command -label "Traffic type" -command "$self select-traffic"
	$mp add command -label "Source node" -command "$self select-src"
	$mp add command -label "Destination node" -command "$self select-dst"
	$mp add command -label "Flow id" -command "$self select-fid"
	$mp add command -label "Reset all" -command "$netModel resetFilter"
	menu $mp1
	set showData_ 1
	set showRouting_ 1
	set showMac_ 1
	$mp1 add checkbutton -label "Data" \
		-variable [$self tkvarname showData_]
	$mp1 add checkbutton -label "Routing" \
		-variable [$self tkvarname showRouting_]
	$mp1 add checkbutton -label "Mac" \
		-variable [$self tkvarname showMac_]


	$m add command -label "Highlight leaf trees" -command \
			"$self show_subtrees"
	$m add checkbutton -label "Show monitors" \
		-variable [$self tkvarname showpanel(monitor)]
        if {$NETWORK_MODEL=="NetworkModel/Auto"} {
	    $m add checkbutton -label "Show autolayout" \
		    -variable [$self tkvarname showpanel(autolayout)]
	} else {
	    $m add checkbutton -label "Show autolayout" -state disabled
	}
	$m add checkbutton -label "Show annotations" \
		-variable [$self tkvarname showpanel(annotate)]
	$m add checkbutton -label "Sync" \
		-variable [$self tkvarname isSync_]
	if ![info exists isSync_] {
		set isSync_ 0
	}
	trace variable isSync_ w "$self on-change-sync"
	pack $mb -side left -padx $padx

	# -----------------------------
	# Analysis Tool
	set mb $w.menu.analysis
	set m $mb.m
	$self instvar analysis_OK
	if { $analysis_OK == 0 } {
	    menubutton $mb -text "Analysis" -menu $m -underline 0 \
		-borderwidth 1 -state disabled
        } else {
	    menubutton $mb -text "Analysis" -menu $m -underline 0 \
		-borderwidth 1
	}

	menu $m
	$m add command -label "Active Sessions" -command "$self active_sessions"
	$m add command -label "Legend ..." -command "$self auto_legend"
	#$m add command -label "Saving ..." -command "$self manual_legend"
	pack $mb -side left -padx $padx
	# ------------------------------

	label $w.menu.name -text $nam_name -font [smallfont] \
		-width 30 -borderwidth 2 -relief groove 
        set windows(title) $w.menu.name
	pack $w.menu.name -side left -fill x -expand true -padx 4 -pady 1

	# Help is moved to main menu
}

Animator instproc select-traffic {} {
   $self tkvar trafficType_
   toplevel .selectTraffic
   set w .selectTraffic
   wm title $w "Enter traffic type (CBR,TCP,...)"
   label $w.label -text "Traffic type:"
   entry $w.entry -width 50 -relief sunken \
                   -textvariable [$self tkvarname trafficType_]
   pack $w.label $w.entry -side left -fill both -expand true
   bind $w.entry <Return> "$self filter-traffic $w"
}

Animator instproc filter-traffic {w} {
   $self tkvar trafficType_
   $self instvar netModel 
   destroy $w
   $netModel traffic_filter $trafficType_
}

Animator instproc select-src {} {
   $self tkvar srcNode_
   toplevel .selectSrc
   set w .selectSrc
   wm title $w "Enter source node"
   label $w.label -text "Source node:"
   entry $w.entry -width 50 -relief sunken \
                   -textvariable [$self tkvarname srcNode_]
   pack $w.label $w.entry -side left -fill both -expand true
   bind $w.entry <Return> "$self filter-src $w"
}

Animator instproc filter-src {w} {
   $self tkvar srcNode_
   $self instvar netModel 
   destroy $w
   $netModel src_filter $srcNode_
}

Animator instproc select-dst {} {
   $self tkvar dstNode_
   toplevel .selectDst
   set w .selectDst
   wm title $w "Enter destination node"
   label $w.label -text "Destination node:"
   entry $w.entry -width 50 -relief sunken \
                   -textvariable [$self tkvarname dstNode_]
   pack $w.label $w.entry -side left -fill both -expand true
   bind $w.entry <Return> "$self filter-dst $w"
}

Animator instproc filter-dst {w} {
   $self tkvar dstNode_
   $self instvar netModel 
   destroy $w
   $netModel dst_filter $dstNode_
}

Animator instproc select-fid {} {
   $self tkvar flowId_
   toplevel .selectFlow
   set w .selectFlow
   wm title $w "Enter flow id"
   label $w.label -text "Flow id:"
   entry $w.entry -width 50 -relief sunken \
                   -textvariable [$self tkvarname flowId_]
   pack $w.label $w.entry -side left -fill both -expand true
   bind $w.entry <Return> "$self filter-fid $w"
}

Animator instproc filter-fid {w} {
   $self tkvar flowId_
   $self instvar netModel 
   destroy $w
   $netModel fid_filter $flowId_
}


Animator instproc selectColor {} {
   $self colorPaletteReq .colorpalette \
        {0000 3300 6600 9900 CC00 FF00} \
        {0000 3300 6600 9900 CC00 FF00} \
        {0000 3300 6600 9900 CC00 FF00} \
        .colorsp
}

Animator instproc colorPaletteReq { name redlist greenlist bluelist replace } {
         $self instvar SharedEnv netModel

	 # Setup
	 set w ${name}
	 if {[winfo exists $w]} {
	            wm deiconify $w
		    raise $w
		    return
         }
	 set SharedEnv($name) $replace
	 eval toplevel $w ""
	 wm protocol $w WM_DELETE_WINDOW "wm withdraw $w"

	 frame $w.f

	 foreach red $redlist {
	         frame $w.f.rcol_${red}
		 foreach green $greenlist {
        		 frame $w.f.grow_${red}${green}
			 foreach blue $bluelist {
			         if { [info exists SharedEnv($w.f.c${red}${green}${blue})] } {
				     frame $w.f.c${red}${green}${blue} \
				           -relief raised -height 2m -width 2m \
					   -highlightthickness 0 \
					   -bd 1 -bg $SharedEnv($w.f.c${red }${green}${blue})
                                 } else {
				     frame $w.f.c${red}${green}${blue} \
				           -relief raised -height 2m -width 2m \
					   -highlightthickness 0 \
					   -bd 1 -bg "#${red}${green}${blue}"
                                 }
				 pack $w.f.c${red}${green}${blue} -side left \
				 -in $w.f.grow_${red}${green} -fill both -expand true
				  bind $w.f.c${red}${green}${blue} <ButtonRelease-1> "
				       $self setcolor %W
                                  "
                          }
                          pack $w.f.grow_${red}${green} -side top \
			       -in $w.f.rcol_${red} -fill both -expand true
                 }
		 pack $w.f.rcol_${red} -side left -in $w.f -fill both \
		      -expand true
	}
	frame $w.f.c_none -width 4m -relief raised -bd 1 \
	      -highlightthickness 0
        pack $w.f.c_none -in $w.f -side left -fill y
	pack $w.f -in $w -expand true -fill both

	# Return
	wm geometry $w 400x100
}

Animator instproc setcolor {w} {
         $self instvar netModel colorarea colorn

	 $w configure -relief raised
	 set colorv [$w cget -bg]
	 set rgb [winfo rgb $w $colorv]
	 set colorn "[$netModel lookupColorName [lindex $rgb 0] \
	     [lindex $rgb 1] [lindex $rgb 2]]"

	set rgb [winfo rgb . $colorn]
	set name "[$netModel lookupColorName [lindex $rgb 0] \
	                 [lindex $rgb 1] [lindex $rgb 2]]"
        $netModel select-color $name

	if {[info exists colorarea] && [winfo exists $colorarea]} {
	    $colorarea delete 0.0 end
	    $colorarea insert 0.0 $colorn
	    catch { $colorarea tag add bgcolor 0.0 end }
	    $colorarea tag configure bgcolor -background $colorn
        }
}

Animator instproc on-change-sync args {
	$self tkvar isSync_
	if $isSync_ {
		$self add-sync $self
	} else {
		$self remove-sync $self
	}
}

Animator instproc set_run { w } {
	$self instvar netView running
	focus $netView
	$self set_forward_dir 1
	if { $running != 1 } {
		$self play 1
		#$self renderFrame
	}
	$self highlight $w.bar.run 1
}

Animator instproc set_back { w } {
	$self instvar netView running
	focus $netView
	$self set_backward_dir 1
	if { $running != 1 } {
		$self play 1
		$self renderFrame
	}
	$self highlight $w.bar.back 1
}

Animator instproc on-rateslider-press {} {
	$self instvar currRate prevRate
	set prevRate $currRate
}

Animator instproc on-rateslider-motion { v } {
	$self instvar timeStep stepDisp
	set timeStep [expr pow(10, $v / 10.)]
	set stepDisp [step_format $timeStep]
}

Animator instproc build-animation-ctrl {w} {
        $self instvar rateSlider granularity timeStep currRate prevRate \
		stepDisp running direction balloon_

	set f [smallfont]
	frame $w.bar -relief groove -borderwidth 2
        frame $w.bar.rate -borderwidth 1 -relief sunken
	scale $w.bar.rate.s -orient horizontal -width 7p \
		        -label "Step:" -font [smallfont]\
			-from -60 -to -1 -showvalue false \
			-relief flat\
			-borderwidth 1 -highlightthickness 0 \
			-troughcolor [option get . background Nam]
        pack $w.bar.rate.s -side top -fill both -expand true
        set rateSlider $w.bar.rate.s
	set granularity [option get . granularity Nam]
	set timeStep [time2real [option get . rate Nam]]
        set stepDisp [step_format $timeStep]
        set currRate [expr 10*log10($timeStep)]
        set prevRate $currRate
	$rateSlider set $currRate
        bind $rateSlider <ButtonRelease-1> "$self set_rate \[%W get\] 1"
	bind $rateSlider <ButtonPress-1> "$self on-rateslider-press"
	bind $rateSlider <B1-Motion> "$self on-rateslider-motion \[%W get\]"
	trace variable stepDisp w "$self displayStep"

	$self tkvar nowDisp
	label $w.bar.timerVal -textvariable [$self tkvarname nowDisp] \
		-width 14 -anchor w -font $f -borderwidth 1 \
		-relief sunken -anchor e

        frame $w.bar.run
	button $w.bar.run.b -bitmap play -borderwidth 1 -relief raised \
		-highlightthickness 0 -font $f  \
		-command "$self set_run $w"
        frame $w.bar.run.f -height 5 -relief sunken \
		-borderwidth 1
        pack $w.bar.run.b -side top -fill both -expand true
        pack $w.bar.run.f -side top -fill x
	$balloon_ balloon_for $w.bar.run.b "play forward" 1000

        frame $w.bar.back
	button $w.bar.back.b -bitmap back -borderwidth 1 -relief raised \
		-highlightthickness 0 -font $f \
		-command "$self set_back $w"
	$balloon_ balloon_for $w.bar.back.b "play backward" 1000

	# hilight running labels as $running changes
	trace variable running w "$self trace_running_handler $w"
	
        frame $w.bar.back.f -height 5 -relief sunken \
		-borderwidth 1
        pack $w.bar.back.b -side top -fill both -expand true
        pack $w.bar.back.f -side top -fill x
        
        frame $w.bar.stop
	button $w.bar.stop.b -bitmap stop -borderwidth 1 -relief raised \
		-highlightthickness 0 -font $f \
		-command "$self stop 1;\
		          $self renderFrame;\
			  $self highlight $w.bar.stop 1"
	$balloon_ balloon_for $w.bar.stop.b "stop" 1000
        frame $w.bar.stop.f -height 5 -relief sunken \
		-borderwidth 1
        pack $w.bar.stop.b -side top -fill both -expand true
        pack $w.bar.stop.f -side top -fill x

        frame $w.bar.rew
	button $w.bar.rew.b -bitmap rew -borderwidth 1 -relief raised \
		-highlightthickness 0 -font $f -command "$self rewind"
	$balloon_ balloon_for $w.bar.rew.b "rewind" 1000
        frame $w.bar.rew.f -height 5 -relief sunken \
                -borderwidth 1
        pack $w.bar.rew.b -side top -fill both -expand true
        pack $w.bar.rew.f -side top -fill x

        frame $w.bar.ff
	button $w.bar.ff.b -bitmap ff -borderwidth 1 -relief raised \
		-highlightthickness 0 -font $f -command "$self fast_fwd"
	$balloon_ balloon_for $w.bar.ff.b "fast forward" 1000
        frame $w.bar.ff.f -height 5 -relief sunken \
                -borderwidth 1
        pack $w.bar.ff.b -side top -fill both -expand true
        pack $w.bar.ff.f -side top -fill x

#	button $w.bar.quit -bitmap eject -borderwidth 1 -relief raised \
#		-highlightthickness 0 -font $f -command "$self stop; $self done"
#	$balloon_ balloon_for $w.bar.quit "Quit Animator" 1000

	pack $w.bar.rate -side right -fill y
	pack $w.bar.timerVal -side right -pady 0 \
		-ipady 1 -padx 1 -fill y
#        pack $w.bar.quit -side right -padx 1 -pady 1 -fill both -expand true
        pack $w.bar.ff -side right -padx 1 -pady 1 -fill both -expand true
        pack $w.bar.run -side right -padx 1 -pady 1 -fill both -expand true
        pack $w.bar.stop -side right -padx 1 -pady 1 -fill both -expand true
        pack $w.bar.back -side right -padx 1 -pady 1 -fill both -expand true
        pack $w.bar.rew -side right -padx 1 -pady 1 -fill both -expand true
#        pack $w.help -side left -padx 1 -pady 1 -fill y 

	pack $w.bar -fill x -expand 1 -side right
        $self instvar prevbutton
        set prevbutton $w.bar.stop

	# start out stopped
	$self highlight $w.bar.stop 1
}

# tb - the name of the edit button
# db1, db2 - names of the buttons to be enable/disabled
Animator instproc toggle-edit { view tb db1 db2 } {
	$self instvar enable_edit_ balloon_
	set enable_edit_ [expr !$enable_edit_]
	if {$enable_edit_} {
		$self clear_view_bind $view
		$self editview_bind $view
		$tb configure -relief sunken
		$db1 configure -state normal
		$db2 configure -state normal
		$tb configure -bitmap "netview"
		$balloon_ balloon_for $db1 "NodeUp (enabled)"
		$balloon_ balloon_for $db2 "NodeDown (enabled)"
	} else {
		$self clear_editview_bind $view
		$self view_bind $view
		$tb configure -relief raised
		$db1 configure -state disabled
		$db2 configure -state disabled
		$tb configure -bitmap "netedit"
		$balloon_ balloon_for $db1 "NodeUp (disabled)"
		$balloon_ balloon_for $db2 "NodeDown (disabled)"
	}
}

# We need $w to put the control pane into, and $mainW to handle "close"
Animator instproc build-zoombar {view w mainW} {
	$self instvar magnification viewOffset balloon_

	set magnification 1.0
	set viewOffset(x) 0.0
	set viewOffset(y) 0.0
    
	frame $w.f
	pack $w.f -side top
	button $w.f.b1 -bitmap "zoomin" -command "$view zoom 1.6" \
			-highlightthickness 0 -borderwidth 1
	pack $w.f.b1 -side top -ipady 3
	button $w.f.b2 -bitmap "zoomout" -command "$view zoom 0.625" \
			-highlightthickness 0 -borderwidth 1
	pack $w.f.b2 -side top -ipady 3

	# Build edit enable/disable button 
	$self instvar enable_edit_
	# By default disable editing
	set enable_edit_ 0
	button $w.f.b3 -bitmap "netedit" -command \
		"$self toggle-edit $view $w.f.b3 $w.f.b4 $w.f.b5" \
		-highlightthickness 1 -borderwidth 1
	pack $w.f.b3 -side top -ipady 3

	# Node size up/down buttons
	$self instvar netModel 
	button $w.f.b4 -bitmap "nodeup" -command \
		"$netModel incr-nodesize 1.1" -state disabled
	button $w.f.b5 -bitmap "nodedown" -command \
		"$netModel decr-nodesize 1.1" -state disabled
	pack $w.f.b4 -side top -ipady 3
	pack $w.f.b5 -side top -ipady 3

	$balloon_ balloon_for $w.f.b1 "ZoomIn"
	$balloon_ balloon_for $w.f.b2 "ZoomOut"
	$balloon_ balloon_for $w.f.b3 "Edit/View"
	$balloon_ balloon_for $w.f.b4 "NodeUp (disabled)"
	$balloon_ balloon_for $w.f.b5 "NodeDown (disabled)"

	if {[winfo name $mainW] != "view"} {
		button $w.f.b6 -bitmap "eject" -command "destroy $mainW" \
				-highlightthickness 0 -borderwidth 1
		pack $w.f.b6 -side top -ipady 3
	}
}

Animator instproc setCurrentTime {time} {
	$self tkvar nowDisp
	set nowDisp $time
}

Animator instproc build-slider {w} {
	$self instvar timeslider timeslider_width timeslider_tag \
		    timeslider_pos nowDisp 

	frame $w.f -borderwidth 2 -relief groove
	pack $w.f -side top -fill x -expand 1
	set timeslider(height) 12
	set timeslider(swidth) 32
	set timeslider(width) 32
	set timeslider_width($w.f.c) 32
	set timeslider(canvas) $w.f.c
	lappend timeslider(canvas_set) $timeslider(canvas)
	set timeslider(frame) $w
	canvas $w.f.c -height $timeslider(height) -width 300 \
			-background white -highlightthickness 0
	pack $w.f.c -side left -fill x -expand 1 -padx 0 -pady 0
	
	label $w.f.c.b -bitmap time -highlightthickness 0 -borderwidth 1 \
			-relief raised
	set timeslider_tag($w.f.c) [$w.f.c create window \
			[expr $timeslider(swidth)/2] 0 -window $w.f.c.b \
			-height 12 -width $timeslider(swidth) -anchor n]
	set timeslider_pos($w.f.c) 0
	bind $w.f.c <ButtonPress-1> "$self timeslidertrough $w.f.c %x %y"
	bind $w.f.c.b <ButtonPress-1> "$self timesliderpress $w.f.c %x %y;break"
	bind $w.f.c.b <ButtonRelease-1> "$self timesliderrelease $w.f.c %x %y"
	bind $w.f.c.b <B1-Motion> "$self timeslidermotion $w.f.c %x %y"
	bind $w.f.c <Configure> "$self timeticks $w.f.c"
}

Animator instproc timeticks { wfc } {
	$self instvar timeslider mintime range timeslider_width \
			timeslider_tag maxtime mid_
	set timeslider(canvas) $wfc

	set st [lindex [split $wfc "."] 2]
	if { [string compare $st "ctrl"] == 0 } {
		set width [winfo width $timeslider(canvas)]
	} else {
                if { [string compare $st "slider"] == 0 } {
                    set namgraphname_ [lindex [split $wfc "."] 1]
                    set width [winfo width .$namgraphname_.main.c]
                } else {                    
	            set width [winfo width $tlw_.$st.main.c]
                }
	}
	if {$width == $timeslider_width($wfc)} { return }
	set timeslider(width) $width
	set timeslider_width($wfc) $width
	$timeslider(canvas) delete ticks

	#we really shouldn't need to do this but there's a redraw bug in the
	#tk canvas that we need to work around (at least in tk4.2) - mjh
	pack forget $timeslider(canvas)
	update
	update idletasks
	pack $timeslider(canvas) -side left -fill x -expand 1 -padx 0 -pady 0

	set width [winfo width $wfc]

	# We need a more adaptive way to draw the ticks. Otherwise for long 
	# and sparse simulations, it'll result in long startup time - haoboy
	set x [expr $timeslider(swidth)/2]
	# Unit of time represented by one pixel
	set tickIncr [expr $range / ($width-$timeslider(swidth))]
	# Should check if range is 0
	if {$range == 0} {
		set intertick [expr ($width - $timeslider(swidth)) / 10]
	} else {
		set intertick [expr ($width-$timeslider(swidth))/(10 * $range)]
	}
	if {$intertick < 2} {
		# This increment should be at least 2 pixel
		set intertick 2
	}
	for {set t $mintime} {$t < ($range+$mintime)} {set t [expr $t+$intertick*$tickIncr]} {
		set intx [expr int($x)]
		$timeslider(canvas) addtag ticks withtag \
			[$timeslider(canvas) create line \
			$intx [expr $timeslider(height)/2] \
			$intx $timeslider(height)]
		set x [expr $x+$intertick]
	}
	set x [expr $timeslider(swidth)/2]
	if {$range == 0} {
		set intertick [expr $width - $timeslider(swidth)]
	} else {
		set intertick [expr ($width-$timeslider(swidth))/($range)]
	}
	if {$intertick < 20} {
		set intertick 20
	}
	for {set t $mintime} {$t < ($range+$mintime)} {set t [expr $t+$intertick*$tickIncr]} {
		set intx [expr int($x)]
		$timeslider(canvas) addtag ticks withtag \
				[$timeslider(canvas) create line \
				$intx 0 $intx $timeslider(height)]
		set x [expr $x+$intertick]
	}

	set wfc_width [winfo width $wfc]

	if {$maxtime > 0} {
		set x [expr ($wfc_width-$timeslider(swidth))*$now/$maxtime]
	} else {
		set x [expr ($wfc_width-$timeslider(swidth))*$now]
	}

	$wfc coords $timeslider_tag($wfc) [expr $x + $timeslider(swidth)/2] 0
}


Animator instproc getmainslidermodel {} {
	$self instvar mslider
	return $mslider
}

Animator instproc setsliderPressed {v} {
	$self instvar sliderPressed
	set sliderPressed $v
}

Animator instproc getrunning {} {
	$self instvar running
	return $running
}

Animator instproc timesliderset {t} {
	$self instvar mslider
	$mslider setcurrenttime $t
	return
}

Animator instproc timesliderpress {w x y} {
	$self instvar timeslider sliderPressed 
	set timeslider(oldpos) $x
	#    set timeslider(width) [winfo width $timeslider(canvas)]
	$w.b configure -relief sunken
	set sliderPressed 1
}

Animator instproc timeslidertrough {w x y} {
	$self instvar timeslider sliderPressed timeslider_pos
	if {$timeslider_pos($w)>$x} {
		$self rewind
	} else {
		$self fast_fwd
    }
}

Animator instproc timeslidermotion {wc x y} {
	$self instvar timeslider mintime range timeslider_tag \
	              timeslider_pos timeslider_width
	$self tkvar nowDisp

	set diff [expr $x - $timeslider(oldpos)]

	set timeslider(canvas) $wc

	$timeslider(canvas) move $timeslider_tag($wc) \
	$diff 0
 
	set timeslider_pos($wc) [expr $timeslider_pos($wc) + $diff]
	if {$timeslider_pos($wc)<0} {
		$timeslider(canvas) move $timeslider_tag($wc) \
		                         [expr 0 - $timeslider_pos($wc)] 0
		set timeslider_pos($wc) 0
	}

	if {$timeslider_pos($wc)>[expr $timeslider_width($wc)-$timeslider(swidth)]} {
		$timeslider(canvas) move $timeslider_tag($wc) \
		                         [expr ($timeslider_width($wc) - $timeslider(swidth)) - \
		                         $timeslider_pos($wc)] 0

		set timeslider_pos($wc) [expr $timeslider_width($wc)-$timeslider(swidth)]
	}

	 set tick 0
	catch {
		set tick [expr 1000.0*$timeslider_pos($wc)/($timeslider_width($wc)-$timeslider(swidth))]
	}
	set now [expr ($tick * $range) / 1000. + $mintime]
	set nowDisp [format %.6f $now]
	set timeslider(tick) $tick

	$self timesliderset $now
}

Animator instproc timesliderrelease {w x y} {
	$self instvar timeslider sliderPressed running

	$self timeslidermotion $w $x $y
	$w.b configure -relief raised
	$self slidetime $timeslider(tick) 1
	set sliderPressed 0
	if $running {
		$self renderFrame
	}
}

Animator instproc nam-relayout {} {
	$self tkvar ITERATIONS KCa KCr Recalc 
	$self instvar now netModel

	$netModel do_relayout $ITERATIONS $KCa $KCr $Recalc
	$self settime $now
	update idletasks
}

Animator instproc set-layout-params { iter kca kcr } {
	$self tkvar ITERATIONS KCa KCr 
	set ITERATIONS $iter
	set KCa $kca 
	set KCr $kcr
}

Animator instproc build-layout {w} {
	$self tkvar ITERATIONS KCa KCr Recalc
	$self instvar NETWORK_MODEL 
	if {$NETWORK_MODEL != "NetworkModel/Auto"} {
		return
	}

	set f [smallfont]
#	frame $w.bar -relief ridge -borderwidth 2
        label $w.label -text "Auto layout:" -font $f
	label $w.label_ca -text "Ca" -font $f
	entry $w.inputca -width 6 -relief sunken \
			-textvariable [$self tkvarname KCa] -font $f
	label $w.label_cr -text "Cr" -font $f
	entry $w.inputcr -width 6 -relief sunken \
			-textvariable [$self tkvarname KCr] -font $f
	label $w.label_iter -text "Iterations" -font $f
	entry $w.inputiter -width 6 -relief sunken \
			-textvariable [$self tkvarname ITERATIONS]

        set Recalc 1
        checkbutton $w.recalc -text Recalc -onvalue 1 -offvalue 0 \
		-variable [$self tkvarname Recalc] -font $f \
		-highlightthickness 0

	$self instvar netModel
	button $w.reset -text reset -relief raised -font $f \
		-command "$netModel reset"

	button $w.relayout -text re-layout -relief raised -font $f \
		-command "$self nam-relayout"

	pack $w.label -side left -padx 1 -pady 1
	pack $w.label_ca -side left -padx 1 -pady 1 -fill x
	pack $w.inputca -side left -padx 1 -pady 1 -fill x
	pack $w.label_cr -side left -padx 1 -pady 1 -fill x
	pack $w.inputcr -side left -padx 1 -pady 1 -fill x

	pack $w.label_iter -side left -padx 1 -pady 1 -fill x
	pack $w.inputiter -side left -padx 1 -pady 1 -fill x
	pack $w.recalc -side left -padx 1 -pady 1 -fill both
	pack $w.reset -side right -padx 1 -pady 1 -fill both
	pack $w.relayout -side right -padx 1 -pady 1 -fill both
#	pack $w.bar -fill x -expand 1

	bind $w.inputca <Return> " $self nam-relayout"
	bind $w.inputcr <Return> " $self nam-relayout"
	bind $w.inputiter <Return> "$self nam-relayout"
}

# we'll build a new annotation listbox for this
Animator instproc build-annotation { w } {
	$self tkvar showpanel
	$self instvar annoBox annoBoxHeight annoJump
        frame $w.spaceral -borderwidth 0 -highlightthickness 0 \
		-height 0 -width 10
        pack $w.spaceral -side top -padx 0 -pady 0
        frame $w.f -borderwidth 0 -highlightthickness 0
	frame $w.f.f
	
	set annoJump 0
	set annoBoxHeight 3
	listbox $w.f.f.a -xscrollcommand "$w.f.f.ah set" \
		-yscrollcommand "$w.f.f2.av set" -height $annoBoxHeight \
		-selectmode single 
	pack $w.f.f.a -fill both -side top -expand true 
	set annoBox $w.f.f.a

	scrollbar $w.f.f.ah -orient horizontal -width 10 -borderwidth 1 \
		-command "$w.f.f.a xview"
	$w.f.f.ah set 0.0 1.0
	pack $w.f.f.ah -side bottom -fill x

	frame $w.f.f2
	pack $w.f.f2 -side left -fill y
	scrollbar $w.f.f2.av -orient vertical -width 10 -borderwidth 1 \
			-command "$w.f.f.a yview"
	$w.f.f2.av set 0.0 1.0
	pack $w.f.f2.av -side top -fill y -expand true
        pack $w.f.f -side left -fill both -expand true
        trace variable showpanel(annotate) w \
		"$self displaypanel $w.f $w.spaceral annotate top both true"

	bind $w.f.f.a <Double-ButtonPress-1> "$self jump_to_annotation $w.f.f.a"
	bind $w.f.f.a <ButtonPress-3> "$self popup_annotation $w \%x \%y"
}

#
# Helper functions
#
Animator instproc displaypanel {panel after name side fill expand args} {
	$self tkvar showpanel

	if {$showpanel($name) == 1} {
		set str "pack $panel -side $side -fill $fill -expand $expand"
		if {$after!=""} {
			set str "$str -after $after"
		}
		eval $str
	} else {
		pack forget $panel
	}
}

Animator instproc closepanel { name } {
	$self tkvar showpanel
	set showpanel($name) 0
}

Animator instproc highlight {w mode} {
    $self instvar prevbutton
    if {$mode==1} {
	$prevbutton.b configure -relief raised
	$prevbutton.f configure -background [option get . background Nam]
	set prevbutton $w
    }
    $w.f configure -background seagreen
    $w.f configure -relief sunken
}

Animator instproc trace_running_handler { w args } { 
	$self instvar direction running

	# $x == $running
	if {$running == 0} {
		$self highlight $w.bar.stop 1 
	} elseif {($direction == 1)} {
		$self highlight $w.bar.run 1
	} else {
		$self highlight $w.bar.back 1
	}
}

#-----------------------------------------------------------------------
# Animator instproc window_bind {w}
#  - Add key and event bindings to the animation window
#-----------------------------------------------------------------------
Animator instproc window_bind {w} {
	bind $w <q> "$self done"
	bind $w <Q> "$self all_done"
	bind $w <Control-w> "$self done"
	bind $w <Control-W> "$self done"
	bind $w <Control-q> "[AnimControl instance] done"
	bind $w <Control-Q> "[AnimControl instance] done"
	bind $w <Control-c> "$self done"

	wm protocol $w WM_DELETE_WINDOW "$self done"

	bind $w <b> "$self back_step"
	bind $w <B> "$self back_step"
	
	bind $w <c> "$self play 1"
	bind $w <C> "$self play 1"
	bind $w <f> "$self fast_fwd"
	bind $w <F> "$self fast_fwd"
	bind $w <n> "$self next_event"
	bind $w <N> "$self next_event"
	bind $w <p> "$self stop 1"
	bind $w <P> "$self stop 1"
	bind $w <r> "$self rewind"
	bind $w <R> "$self rewind"
	bind $w <u> "$self time_undo"
	bind $w <U> "$self time_undo"
	bind $w <x> "$self rate_undo"
	bind $w <X> "$self rate_undo"
	bind $w <period> "$self change_rate 1"
	bind $w <greater> "$self change_rate 1"
	bind $w <comma> "$self change_rate 0"
	bind $w <less> "$self change_rate 0"
}

Animator instproc view_bind {netView} {
	# We need these keys in input boxes, so only bind them to .view
	bind $netView <space> "$self toggle_pause"
	bind $netView <Control-d> "$self done"
	bind $netView <Return> "$self single_step"
	bind $netView <BackSpace> "$self back_step"
	bind $netView <Delete> "$self back_step"
	bind $netView <0> "$self reset"

	bind $netView <ButtonPress-1> "$self start_info $netView \%x \%y left"
	bind $netView <ButtonPress-3> "$self start_info $netView \%x \%y right"
	bind $netView <ButtonPress-2> "$self view_drag_start $netView \%x \%y"
	bind $netView <B2-Motion> "$self view_drag_motion $netView \%x \%y"
#	bind $netView <ButtonRelease-3> "$self end_info $netView"
}

Animator instproc clear_view_bind { netView } {
	bind $netView <ButtonPress-1> ""
	bind $netView <ButtonPress-3> ""
	bind $netView <ButtonPress-2> ""
	bind $netView <B2-Motion> ""
}
