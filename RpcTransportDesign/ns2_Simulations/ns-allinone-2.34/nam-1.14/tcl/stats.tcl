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
# $Header: /cvsroot/nsnam/nam-1/tcl/stats.tcl,v 1.16 2002/05/17 20:55:37 buchheim Exp $

Animator instproc tracehooktcl { e } {
 
    # notify observers
    $self notifyObservers $e
}

Animator instproc update_statsview { s event } {

    $self instvar colorset highest_seq timeslider maxtime colorname subView \
	subViewRange plotdatax plotdatay plotmark

    set s_len [string length $s]
    set session_id [string index $s [expr $s_len-1]]

    # Only process THIS window
    set sid [get_trace_item "-S" $event]

    if {$sid != $session_id} {return}

    # main view
    set extinfo [get_trace_item "-x" $event]
    set time [get_trace_item "-t" $event]
    set seqno [lindex $extinfo 2]
    set colorid [get_trace_item "-a" $event]
    set mid [get_trace_item "-m" $event]

    if { $mid == "" } {return}

    set cnt 0
    while {[info exists plotdatax($sid.$cnt)]} {
	$self plot_xy $s.main.c $time $seqno 0 $maxtime 0 $highest_seq($sid) \
		$colorid $plotmark($sid.$mid) 0
        incr cnt
    }

    # subview
    foreach subviews $subView(#$session_id) {
  	if {![winfo exists $subviews]} {continue}
	    
	set minx [lindex $subViewRange($subviews) 0]
	set maxx [lindex $subViewRange($subviews) 1]
	set miny [lindex $subViewRange($subviews) 2]
	set maxy [lindex $subViewRange($subviews) 3]
	$self plot_xy $subviews.main.c $time $seqno $minx $maxx \
		 $miny $maxy $colorid $plotmark($sid.$mid) 1

    }
}


#change nam window status 
Animator instproc new_waitingtext { msg } {
    $self instvar windows 
    $windows(title) configure -text "Please wait - $msg"
    update
}

# restore nam window status
Animator instproc restore_waitingtext {} {
    $self instvar windows nam_name
    $windows(title) configure -text $nam_name
}

Animator instproc active_sessions {} {
    $self instvar analysis_ready colorset colorindex session_id colorname \
		    netModel analysis_flag windows nam_name tlw_ cache_ready
    #processing tracefile if necessary
    
    if {[string compare $analysis_flag "0"] == 0} {
	set analysis_flag 1
    }

    set w $tlw_.activesessions

    if {[winfo exists $w]} {
    	raise $w
        return
    } 
    $windows(title) configure -text "Please wait ... "
    update
    # good timing
    # check if analysis ready
    if { $analysis_ready == 1 && $cache_ready == 0 } {
        $self cache_plot
	set cache_ready 1
    }
    $windows(title) configure -text $nam_name

    #processing tracefiel if necessary

    toplevel $w

    wm title $w "Current Active Sessions"

    bind $w <Control-w> "destroy $w"
    bind $w <Control-W> "destroy $w"
    bind $w <Control-q> "[AnimControl instance] done"
    bind $w <Control-Q> "[AnimControl instance] done"

    frame $w.f 
    pack $w.f -side top

    label $w.f.label_left -text "LEGEND"
    label $w.f.label_right -text "SESSIONS"

    for {set i 0} { $i < $colorindex } { incr i} {
    
	label $w.f.label_left$i -text "    " -bg $colorname($i);
	set stats_title $session_id($i)
	button $w.f.button$i -text $session_id($i) \
		-command "$self make_mainwin \"$i\""

    }

    grid config $w.f.label_left -column 0 -row 0 -sticky "n"
    grid config $w.f.label_right -column 1 -row 0 -sticky "n"

    for {set i 0} { $i < $colorindex } { incr i} {
	grid config $w.f.label_left$i -column 0 -row [expr $i+1] \
		-sticky "snew"
	grid config $w.f.button$i -column 1 -row [expr $i+1] \
		-sticky "snew"
    }
}

Animator instproc auto_legend {} {

    $self instvar Mcnt plotmarks colorname filter_id filtercolor_id colorindex tlw_ 

    set w $tlw_.autolegend
 
    if {[winfo exists $w]} {
    	raise $w
        return
    }

    toplevel $w
    wm title $w "Current Filter Legend"

    bind $w <Control-w> "destroy $w"
    bind $w <Control-W> "destroy $w"
    bind $w <Control-q> "[AnimControl instance] done"
    bind $w <Control-Q> "[AnimControl instance] done"
 
    frame $w.f
    pack $w.f -side top
 
    label $w.f.label_left -text "LEGEND"
    label $w.f.label_right -text "EXPLANATION"
 
    for {set i 0} { $i < $Mcnt } { incr i} {

#    if { $i == 2 } { 
#	    set i [expr $i+$colorindex-2]
#        }

#	if { $i > 1 } {
#            label $w.f.label_left$i -bitmap $plotmarks($i) -fg $colorname($i)
#	} else {
#	    label $w.f.label_left$i -bitmap $plotmarks($i) -fg $colorname(0)
#	}

	label $w.f.label_left$i -bitmap $plotmarks($i) -fg $colorname($filtercolor_id($i))

        label $w.f.button$i -text $filter_id($i)
 
    }
 
    grid config $w.f.label_left -column 0 -row 0 -sticky "n"
    grid config $w.f.label_right -column 1 -row 0 -sticky "n"
 
    for {set i 0} { $i < $Mcnt } { incr i} {
#        if { $i == 2 } { 
#            set i [expr $i+$colorindex-2]
#        }

        grid config $w.f.label_left$i -column 0 -row [expr $i+1] \
                -sticky "snew"
        grid config $w.f.button$i -column 1 -row [expr $i+1] \
                -sticky "snew"
    }

}

Animator instproc ScrolledCanvas { c width height region } {
        frame $c
        canvas $c.canvas -width $width -height $height \
                -scrollregion $region \
                -xscrollcommand [list $c.xscroll set] \
                -yscrollcommand [list $c.yscroll set]
        scrollbar $c.xscroll -orient horizontal \
                -command [list $c.canvas xview]
        scrollbar $c.yscroll -orient vertical \
                -command [list $c.canvas yview]
        pack $c.xscroll -side bottom -fill x
        pack $c.yscroll -side right -fill y
        pack $c.canvas -side left -fill both -expand true
        pack $c -side top -fill both -expand true
        return $c.canvas
}

Animator instproc build.m0 { w } {
    bind $w <Configure> "$self xtimeticks $w"
}

Animator instproc xtimeticks { w } {
    $self instvar timeslider mintime range timeslider_width

    set width [winfo width $w]
    set height [winfo height $w]

    $w delete ticks
    
    set x [expr $timeslider(swidth)/2]
    set intertick [expr ($width-$timeslider(swidth))/(10 * $range)]
    for {set t $mintime} {$t < ($range+$mintime)} {set t [expr $t+0.1]} {
        set intx [expr int($x)]
        $w addtag ticks withtag \
                [$w create line \
                $intx [expr $timeslider(height)/2 + $height*9/10] $intx [expr $timeslider(height) + $height*9/10]]
        set x [expr $x+$intertick]
    }
    
    set orx [expr $timeslider(swidth)/2]
    $w addtag ticks withtag \
        [$w create line $orx [expr $timeslider(height)+$height*9/10] $x [expr $timeslider(height)+$height*9/10]]
    $w addtag ticks withtag \
  	[$w create line $orx [expr $timeslider(height)+$height*9/10] $orx 0]]

    set x [expr $timeslider(swidth)/2]
    set intertick [expr ($width-$timeslider(swidth))/($range)]
    for {set t $mintime} {$t < ($range+$mintime)} {set t [expr $t+1]} {
        set intx [expr int($x)]
        $w addtag ticks withtag \
                [$w create line \
                $intx [expr $timeslider(height) + $height*9/10] $intx [expr $height*9/10]]
        set x [expr $x+$intertick]
    }
}


Animator instproc make_mainwin { sid } {
    $self tkvar model_ 
    $self instvar session_id
    $model_($sid) startview $session_id($sid)
}


# namgraph
# pre-process for nam analysis

Animator instproc nam_analysis { tracefile } {
    $self instvar analysis_OK analysis_ready trace cache_ready count

    set stream [new NamStream $tracefile]
    set line [$stream gets]
    set time [get_trace_item "-t" $line]
    set count 0

    #Handle nam version, *SHOULD NOT* assume the first line is V line

    # skip all beginning non "*" events
    while {([$stream eof]==0)&&([string compare $time "*"]!=0)} {
        set line [$stream gets]
        set time [get_trace_item "-t" $line] 
    }

    while {([$stream eof]==0)&&([string compare $time "*"]==0) } {
        set cmd [lindex $line 0]
            # Skip comment lines
            if [regexp {^\#} $cmd] {
                    continue
            }

        switch "$cmd" {
            "V" {
                 $self handle_version $line
             }
	    "N" {
		 $self handle_analysis $line
	     }
 	    "c" {
		 $self handle_colorname $line
	     }
        }
        set count [expr $count + 1]
        set line [$stream gets]
        set time [get_trace_item "-t" $line]
    }
    $stream close

    if { $count==0 } {
         puts "*** !!! ***"
         puts "nam cannot recognize the trace file $tracefile"
         puts "Please make sure that the file is not empty and it is a nam trace"
         puts "***********"
         exit
    }

    # old nam, skip it
    if { $analysis_OK == 0 } { 
         puts "You are using the tracefile format older than 1.0a5"
	 puts "which will not allow you to run namgraph"
         return
    }

    set cache_ready 0
}

Animator instproc cache_plot { } {
    $self instvar tracefile plotdatax plotdatay plotmark plotmarks plotcolor

    if ![info exists plotmarks] {
	    # Initialize - loop assign ?
	    set plotmarks(0) mark1
	    set plotmarks(1) mark2
	    set plotmarks(2) mark3
	    set plotmarks(3) mark4
	    set plotmarks(4) mark5
	    set plotmarks(5) mark6
	    set plotmarks(6) mark7
	    set plotmarks(7) mark8
            set plotmarks(8) mark1
            set plotmarks(9) mark2
            set plotmarks(10) mark3
            set plotmarks(11) mark4
            set plotmarks(12) mark5
            set plotmarks(13) mark6
            set plotmarks(14) mark7
            set plotmarks(15) mark8

    }

    set file [new NamStream $tracefile]

#    set file [open $tracefile "r"]
    $self tkvar model_

    while {[$file eof]==0} {
	set line [$file gets]
	set time [get_trace_item "-t" $line]

        if {[string compare $time "*"]==0 } {continue}

	set Sid [get_trace_item "-S" $line]
	set mid [get_trace_item "-m" $line]
	set pid [get_trace_item "-p" $line]
	set fid [get_trace_item "-f" $line]
	set yvalset [get_trace_item "-y" $line]

	set yval [lindex $yvalset 0]
	set ymark [lindex $yvalset 1]

	if { $mid == "" } {continue}

	set plotmark($Sid.$mid) $plotmarks($mid)
	set plotcolor($Sid.$mid) $fid

        #NEW MCV stuff. It will replace the above code finally

	if ![info exists model_($Sid)] {
	    #create a new namgraph model
	    set model_($Sid) [new NamgraphModel $Sid $self]
	    # Attach this model to Animator
	    $self addObserver $model_($Sid)
	}

	set current_model $model_($Sid)
	$current_model adddata $self $mid $time $yval $ymark

    }
    $file close
}

Animator instproc handle_analysis { line } {

    $self instvar session_id filter_id colorname highest_seq filtercolor_id \
	  ymark

    set index [get_trace_item "-S" $line]
    set findex [get_trace_item "-F" $line]
    set title [get_trace_item "-n" $line]
    set hseq [get_trace_item "-h" $line]
    set mindex [get_trace_item "-M" $line]
    set groupm [get_trace_item "-m" $line]
    set proto [get_trace_item "-p" $line]
	
    #session info
    if { $index != "" && $title != "" } {
        set session_id($index) $title
	set proto_id($index) $proto
    }
    if { $index != "" && $hseq != "" } {
	set highest_seq($index) $hseq
    }
    #filter info
    if { $findex != "" } {
        set filter_id($mindex) $title
	set filtercolor_id($mindex) $findex
    }
    
}

Animator instproc handle_colorname { line } {
    $self instvar colorname
    set index [get_trace_item "-i" $line] 
    set colorn [get_trace_item "-n" $line]
    set colorname($index) $colorn
}

Animator instproc handle_version { line } {

    $self instvar analysis_OK nam_version analysis_ready colorindex \
	highest_seq Mcnt

    set nam_version [get_trace_item "-v" $line]
    if { $nam_version >= "1.0a5" } {
	set analysis_OK 1
    }
    set analysis_ready [get_trace_item "-a" $line]
    set colorindex [get_trace_item "-c" $line]
    #set highest_seq [get_trace_item "-h" $line]
    #set Fcnt [get_trace_item "-F" $line]
    #set Fcnt [expr $Fcnt+2]
    set Mcnt [get_trace_item "-M" $line]
}

Animator instproc viewgraph { object graphtype tracefile } {
    $self instvar netView now vslider windows nam_name graphs tlw_ 

    if {$object==""} {return}
    set graphtrace [new Trace $tracefile $self]
    set netgraph ""
    switch [lindex $object 0] {
	l {
	    set netgraph [new LinkNetworkGraph]
	    switch $graphtype {
		"bw" {
		    $netgraph bw [lindex $object 1] [lindex $object 2]
		}
		"loss" {
		    $netgraph loss [lindex $object 1] [lindex $object 2]
		}
	    }
	}
	f {
		set netgraph [new FeatureNetworkGraph]
		$netgraph feature [lindex $object 1] [lindex $object 2] [lindex $object 3]
        }
    }

    if {$netgraph==""} {
	return
    }
    set name [lindex $object 0]_[lindex $object 1]_[lindex $object 2]_$graphtype

    if {[winfo exists $tlw_.graph.f$name]==1} {
	return
    }

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
	  "$self viewgraph_label \"[$self viewgraph_name $object $graphtype]\" \
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

Animator instproc viewgraph_label {info win where netgraph} {
    $self instvar tlw_

    if {[winfo exists $win.lbl]==0} {
	frame $win.lbl -borderwidth 2 -relief groove
	button $win.lbl.hide -text "Hide" -relief raised -borderwidth 1 \
		-highlightthickness 0 \
		-command "destroy $win;\
		$self rm_list_entry graphs $netgraph;\
		if {\[winfo children $tlw_.graph\]=={}} {destroy $tlw_.graph}"
	pack $win.lbl.hide -side left
	label $win.lbl.l -text $info -font [smallfont]
	pack $win.lbl.l -side left
    }
    catch {
	pack $win.lbl -side left -after $where -fill y
	pack forget $where
	bind $win.lbl <Leave> \
	    "pack $where -side left -before $win.lbl;pack forget $win.lbl"
    }
}

Animator instproc rm_list_entry {var value} {
    $self instvar $var
    set res ""
    set lst [set [set var]]
    foreach el $lst {
	if {[string compare $el $value]!=0} {
	    lappend res $el
	}
    }
    set [set var] $res
}

Animator instproc viewgraph_name {name graphtype} {
    set type [lindex $name 0]
    switch $type {
	"l" {
	    switch $graphtype {
		"bw" {
		    return "Bandwidth used on link [lindex $name 1]->[lindex $name 2]"
		}
		"loss" {
		    return "Packets dropped on link [lindex $name 1]->[lindex $name 2]"
		}
	    }
	}
	"f" {
	    return "[lindex $name 2] [lindex $name 3]"

	}
    }
    return unknown
}

