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
# $Header: /cvsroot/nsnam/nam-1/tcl/monitor.tcl,v 1.9 2001/08/10 01:45:52 mehringe Exp $ 

set montab(l) graph
set montab(n) node
set montab(p) monitor
set montab(d) monitor
set montab(r) monitor
set montab(a) monitor

Animator instproc graph_info { netView x y } {
    $self instvar running resume tracefile
    $self tkvar nowDisp
    catch {destroy $netView.f}
    frame $netView.f -relief groove -borderwidth 2
    set name [$netView getname $nowDisp $x $y]
    set type [lindex $name 0]
    switch $type {
	"l" {
	    set e1 [lindex $name 1]
	    set e2 [lindex $name 2]
	    frame $netView.f.f1 -borderwidth 2 -relief groove
	    pack $netView.f.f1 -side left
	    label $netView.f.f1.l -text "Graph bandwidth" -font [smallfont]
	    pack $netView.f.f1.l -side top
	    frame $netView.f.f1.f -borderwidth 0
	    pack $netView.f.f1.f -side top
	    button $netView.f.f1.f.b2 -relief raised -highlightthickness 0 \
		    -text "Link $e1->$e2" -font [smallfont]\
		    -command "$self end_info $netView;\
		      $self viewgraph \"l $e1 $e2\" bw $tracefile"
	    pack $netView.f.f1.f.b2 -side left -fill x -expand true
	    button $netView.f.f1.f.b3 -relief raised -highlightthickness 0 \
		    -text "Link $e2->$e1" -font [smallfont] \
		    -command "$self end_info $netView;\
		      $self viewgraph \"l $e2 $e1\" bw $tracefile"
	    pack $netView.f.f1.f.b3 -side left -fill x -expand true

	    frame $netView.f.f2 -borderwidth 2 -relief groove
	    pack $netView.f.f2 -side left
	    label $netView.f.f2.l -text "Graph loss" -font [smallfont]
	    pack $netView.f.f2.l -side top
	    frame $netView.f.f2.f -borderwidth 0
	    pack $netView.f.f2.f -side top
	    button $netView.f.f2.f.b2 -relief raised -highlightthickness 0 \
		    -text "Link $e1->$e2" -font [smallfont]\
		    -command "$self end_info $netView;\
		      $self viewgraph \"l $e1 $e2\" loss $tracefile"
	    pack $netView.f.f2.f.b2 -side left -fill x -expand true
	    button $netView.f.f2.f.b3 -relief raised -highlightthickness 0 \
		    -text "Link $e2->$e1" -font [smallfont] \
		    -command "$self end_info $netView;\
		      $self viewgraph \"l $e2 $e1\" loss $tracefile"
	    pack $netView.f.f2.f.b3 -side left -fill x -expand true
	}
    }
    button $netView.f.d -relief raised -highlightthickness 0 \
	    -text "Dismiss" -font [smallfont]\
	    -command "$self end_info $netView"
    pack $netView.f.d -side left  -anchor sw

    place_frame $netView $netView.f $x $y
}

#-----------------------------------------------------------------------
# Animator instproc start_info { netView x y key_}
#   - Display a popup window with additional information about
#     some network objects
#-----------------------------------------------------------------------
Animator instproc start_info { netView x y key_} {
  $self instvar running resume tracefile 
  $self instvar fid pktType eSrc eDst
  $self tkvar nowDisp
  global montab

  # If the animation is playing save that info so we can resume after the
  # info window is closed
  if {($running) || ([winfo exists $netView.f] && ($resume == 1))} {
    set resume 1 
  } else {
    set resume 0
  }

  # Stop the animation while the popup info window is displayed
  $self stop 1

  # Get information about the clicked on object (if any)
  set text [string trim [$netView info $nowDisp $x $y] "\n"]
  if { [string length $text] > 0 } {
    set name [$netView getname $nowDisp $x $y]
    set fid [$netView getfid $nowDisp $x $y]
    set pktType [$netView gettype $nowDisp $x $y]
    set eSrc [$netView getesrc $nowDisp $x $y]
    set eDst [$netView getedst $nowDisp $x $y]
    set type [lindex $name 0]

    if {$type==""} {
      puts "got an empty object name returned from object $text!"
      return
    }
    catch {destroy $netView.f}

    # Create frame for popup window
    frame $netView.f -relief groove -borderwidth 2

    # Add text info message to window
    message $netView.f.msg -width 8c -text $text \
                           -font [option get . smallfont Nam]
    pack $netView.f.msg -side top

    # Add frame to hold buttons
    frame $netView.f.f
    pack $netView.f.f -side top -fill x -expand true

    if {$montab($type)=="monitor"} {
      button $netView.f.f.b -relief raised -highlightthickness 0 \
                            -text "Monitor" \
                            -command "$self monitor $netView $nowDisp $x $y; \
                                      $self end_info $netView"
      pack $netView.f.f.b -side left -fill x -expand true
      button $netView.f.f.b1 -relief raised -highlightthickness 0 \
                             -text "Filter" \
                             -command "$self filter_menu ; \
                                       $self end_info $netView"
      pack $netView.f.f.b1 -side left -fill x -expand true

    } elseif {$montab($type)=="graph"} {
      button $netView.f.f.b2 -relief raised -highlightthickness 0 \
                             -text "Graph" \
                             -command "$self graph_info $netView $x $y"
      pack $netView.f.f.b2 -side left -fill x -expand true

    } elseif {$montab($type)=="node"} {
      # Node Buttons
      set node_id [lindex $name 1]
      button $netView.f.f.b3 -relief raised -highlightthickness 0 \
                             -text "Filter" \
                             -command "$self node_filter_menu $node_id; \
                                       $self end_info $netView"
      pack $netView.f.f.b3 -side left -fill x -expand true

      set node_tclscript [$netView get_node_tclscript $node_id]
      if {$node_tclscript != ""} {
        set node_tclscript_label [$netView get_node_tclscript_label $node_id]
        button $netView.f.f.b4 -relief raised -highlightthickness 0 \
                               -text $node_tclscript_label \
                               -command $node_tclscript
        pack $netView.f.f.b4 -side left -fill x -expand true
      }

    }

    button $netView.f.f.d -relief raised -highlightthickness 0 \
                          -text "Dismiss" \
                          -command "$self end_info $netView"

    pack $netView.f.f.d -side left -fill x -expand true
    place_frame $netView $netView.f $x $y
  } else {
    $self end_info $netView
  }
}

Animator instproc node_filter_menu {nodeId} { 
  $self tkvar setSrc_ setDst_ 
  toplevel .nodeFilterMenu
  set w .nodeFilterMenu
  wm title $w "Only show packet with this node as its "
  checkbutton $w.c1  -text "Source address" \
                  -variable [$self tkvarname setSrc_] -anchor w
  checkbutton $w.c2  -text "Destination address" \
                  -variable [$self tkvarname setDst_] -anchor w
  button $w.b1  -relief raised -highlightthickness 0 \
         -text "Close" -command "$self set_node_filters $w $nodeId"
  pack $w.c1 $w.c2 $w.b1 -side top -fill x -expand true
}


Animator instproc filter_menu {} { 
  $self instvar colorarea colorn
  $self tkvar setFid_ setSrc_ setDst_ setTraffic_ filter_action
  toplevel .filterMenu
  set w .filterMenu
  wm title $w "Only show packet with the same"
  frame $w.f1 -borderwidth 2 -relief groove
  pack $w.f1 -side top -fill x
  checkbutton $w.f1.c1  -text "Flow id" \
                  -variable [$self tkvarname setFid_] -anchor w
  checkbutton $w.f1.c2  -text "Source address" \
                  -variable [$self tkvarname setSrc_] -anchor w
  checkbutton $w.f1.c3  -text "Destination address" \
                  -variable [$self tkvarname setDst_] -anchor w
  checkbutton $w.f1.c4  -text "traffic type" \
                  -variable [$self tkvarname setTraffic_] -anchor w
  pack $w.f1.c1 $w.f1.c2 $w.f1.c3 $w.f1.c4 -side top -fill x -expand true
  frame $w.f2 -borderwidth 2 -relief groove
  pack $w.f2 -side top -fill x
  frame $w.f2.a1 -borderwidth 2 -relief groove
  radiobutton $w.f2.a1.r  -text "Color packet" \
              -variable [$self tkvarname filter_action] -value color -anchor w
  button $w.f2.a1.b  -relief raised -highlightthickness 0 \
         -text "Color" -command "$self selectColor"
  set colorarea $w.f2.a1.c
  text $w.f2.a1.c -width 15 -height 1
  if {[info exists colorn] } { 
    $colorarea insert 0.0 $colorn
    catch { $colorarea tag add bgcolor 0.0 end }
    $colorarea tag configure bgcolor -background $colorn
  } else {
    $colorarea insert 0.0 "black" 
  }
  pack $w.f2.a1.r $w.f2.a1.b $w.f2.a1.c -side left -fill y -expand true
  radiobutton $w.f2.a2  -text "Show only packet" \
              -variable [$self tkvarname filter_action] -value showpkt -anchor w
  radiobutton $w.f2.a3  -text "Don't show packet" \
              -variable [$self tkvarname filter_action] -value hidepkt -anchor w
  pack $w.f2.a1 $w.f2.a2 $w.f2.a3  -side top -fill x -expand true
  button $w.b1  -relief raised -highlightthickness 0 \
         -text "Close" -command "$self set_filters $w"
  pack $w.b1 -side top -fill x -expand true
  set filter_action color
}

Animator instproc resetAll {} { 
  $self instvar netModel 
  $self tkvar setFid_ setSrc_ setDst_ setTraffic_
  $self tkvar setFidC_ setSrcC_ setDstC_ setTrafficC_
  set setFid_ 0
  set setSrc_ 0
  set setDst_ 0
  set setTraffic_ 0
  set setFidC_ 0
  set setSrcC_ 0
  set setDstC_ 0
  set setTrafficC_ 0

  $netModel resetFilter

}


Animator instproc set_filters {w} { 
  $self instvar fid netModel pktType eSrc eDst 
  $self tkvar setFid_ setSrc_ setDst_ setTraffic_ filter_action
  if {$filter_action=="showpkt"} {
     if $setFid_ {
        $netModel fid_filter $fid
     } else {
        $netModel fid_filter -1
     }
     if $setTraffic_ {
        $netModel traffic_filter $pktType
     } else {
        $netModel traffic_filter ""
     }
     if $setSrc_ {
        $netModel src_filter $eSrc
     } else {
        $netModel src_filter -1
     }
     if $setDst_ {
        $netModel dst_filter $eDst
     } else {
        $netModel dst_filter -1
     } 
  } elseif {$filter_action=="color"} {
     if $setFid_ {
        $netModel color-fid $fid
     } else {
        $netModel color-fid -1
     }
     if $setTraffic_ {
        $netModel color-traffic $pktType
     } else {
        $netModel color-traffic ""
     }
     if $setSrc_ {
        $netModel color-src $eSrc
     } else {
        $netModel color-src -1
     }
     if $setDst_ {
        $netModel color-dst $eDst
     } else {
        $netModel color-dst -1
     }
  } elseif {$filter_action=="hidepkt"} {
     if $setFid_ {
        $netModel hide-fid $fid
     } else {
        $netModel hide-fid -1
     }
     if $setTraffic_ {
        $netModel hide-traffic $pktType
     } else {
        $netModel hide-traffic ""
     }
     if $setSrc_ {
        $netModel hide-src $eSrc
     } else {
        $netModel hide-src -1
     }
     if $setDst_ {
        $netModel hide-dst $eDst
     } else {
        $netModel hide-dst -1
     }
  }
  destroy $w
}


Animator instproc set_node_filters {w nodeId} { 
  $self instvar netModel 
  $self tkvar setSrc_ setDst_ 
  if $setSrc_ {
     $netModel src_filter $nodeId
  } else {
     $netModel src_filter -1
  }
  if $setDst_ {
     $netModel dst_filter $nodeId
  } else {
     $netModel dst_filter -1
  }
  destroy $w
}


#-----------------------------------------------------------------------
# Animator instproc end_info {netView} 
#    - Delete the more info popup window
#-----------------------------------------------------------------------
Animator instproc end_info {netView} { 
  $self instvar resume
  catch { destroy $netView.f }
  if $resume "$self play 1"
}


Animator instproc monitor {netView now x y} {
	$self instvar windows 
	$self tkvar showpanel

	set mon [$netView new_monitor $now $x $y]
	set showpanel(monitor) 1
	if {$mon>=0} {
		$self create_monitor $netView $mon $now
	}
}

# Used exclusively by annotation command
Animator instproc monitor_agent {node agent} {
	$self instvar netViews netModel 
	$self tkvar nowDisp showpanel

	set netView [lindex $netViews 0]
	set mon [$netModel new_monitor_agent $node $agent]
	set showpanel(monitor) 1
	if {$mon>=0} {
		$self create_monitor $netView $mon $nowDisp
	}
}

Animator instproc create_monitor {netView mon now} {
    $self instvar windows monitors maxmon
    frame $windows(monitor).f$maxmon -borderwidth 2 -relief groove
    pack $windows(monitor).f$maxmon -side left -fill y
    set monitors($mon) $windows(monitor).f$maxmon
    set w $windows(monitor).f$maxmon
    message $w.l -aspect 300 -text "\[$mon\] [$netView monitor $now $mon]" \
	    -anchor nw -font [option get . smallfont Nam]
    pack $w.l -side top -anchor w -fill both -expand true
#    button $w.b -text "Hide" -borderwidth 1 \
#	    -command "$self delete_monitor $netView $mon"
#    pack $w.b -side top
    bind $w.l <Enter> \
	    "$w.l configure -background [option get . activeBackground Nam]"
    bind $w.l <Leave> \
	    "$w.l configure -background [option get . background Nam]"
    bind $w.l <1> "$self delete_monitor $netView $mon"
    incr maxmon
}

Animator instproc update_monitors {netView now} {
    $self instvar monitors
    foreach mon [array names monitors] {
	set text [$netView monitor $now $mon]
	if {$text==""} {
	    $self delete_monitor $netView $mon
	} else {
	    set text "\[$mon\] $text"
	    if {[$monitors($mon).l configure -text]!=$text} {
		$monitors($mon).l configure -text "$text"
	    }
	}
    }
}

Animator instproc delete_monitor {netView mon} {
    $self instvar monitors
    $netView delete_monitor $mon
    catch {destroy $monitors($mon)}
    unset monitors($mon)
    $self redrawFrame
}
