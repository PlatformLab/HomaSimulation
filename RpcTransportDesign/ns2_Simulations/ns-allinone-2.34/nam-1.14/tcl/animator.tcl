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
# $Header: /cvsroot/nsnam/nam-1/tcl/animator.tcl,v 1.48 2007/02/18 22:44:54 tom_henderson Exp $

#-----------------------------------------------------------------------
# Animator instproc init { trace_file args }
#   - Do initializations that are done only once for all netmodels
#-----------------------------------------------------------------------
Animator instproc init { trace_file args } {
  # Move all global variables to instvar
  $self instvar windows trace tracefile \
        prevRate rateSlider currRate netModel \
        running peers peerName granularity direction \
        nam_name graphs NETWORK_MODEL nam_record_filename \
        nam_record_frame id_ sliderPressed maxmon analysis_flag \
        colorname fcolorname now mintime maxtime prevTime range \
        welcome analysis_OK analysis_ready nam_version viewctr \
        plotmarks maxtimeflag pipemode enable_edit_ showenergy_ \
        backward do_validate_nam

  $self instvar observerlist_
  set observerlist_ ""

  # Create shadow
  $self next

  set id_ [Animator set id_]
  Animator set id_ [incr id_]

  Animator instvar SyncAnimator_
  if ![info exists SyncAnimator_] {
    set SyncAnimator_ {}  ;# Empty list
  }
  $self tkvar isSync_
  set isSync_ 0

  set tracefile $trace_file
  set nam_name $tracefile
  
  set showenergy_ 0
  set analysis_flag 0
  set analysis_OK 0
  set analysis_ready 0
  set nam_version 0
  set welcome "                              Welcome to NamGraph 1.14                               "

  # network model used. can be customized in .nam.tcl in current 
  # directory
  set NETWORK_MODEL NetworkModel

  # these next two should be bound variables (or something better)
  set nam_record_filename nam
  set nam_record_frame 0

  set sliderPressed 0
  set direction 1
  set running 0

  # initialize variables for snapshot
  set backward 0
  set do_validate_nam 0

  set viewctr 0
  set maxmon 0

  # Color database used by Statistics module
  set colorname(0) black
  set colorname(1) navy
  set colorname(2) cornflowerblue
  set colorname(3) blue
  set colorname(4) deepskyblue
  set colorname(5) steelblue
  set colorname(6) dodgerblue
  set colorname(7) darkolivegreen

  set fcolorname(0) red
  set fcolorname(1) brown
  set fcolorname(2) purple
  set fcolorname(3) orange
  set fcolorname(4) chocolate
  set fcolorname(5) salmon
  set fcolorname(6) greenyellow
  set fcolorname(7) gold
  
  #plotmarks
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

  set graphs ""
  set statsViews ""

  $self infer-network-model $tracefile
  $self nam_analysis $tracefile

  set netModel [new $NETWORK_MODEL $self $tracefile]

  if [catch {
    set trace [new Trace $tracefile $self]
    set now [$trace mintime]
    set mintime $now
    set maxtime [expr [$trace maxtime] + .05]
    set pipemode 0
    if { [$trace maxtime] == 0 } {
        set maxtime 3.0
        set pipemode 1
        set maxtimeflag 0 
    }
    set range [expr $maxtime - $mintime]
    set prevTime $mintime
    $trace connect $netModel
  } errMsg] {
    error "$errMsg" 
  }

  $self build-ui

  # build-ui{} sets currRate. We allow setting animation rate from 
  # an initialization file using an "one-time" class variable
  Animator instvar INIT_RATE_ INIT_TIME_
  if [info exists INIT_RATE_] {
    set currRate [expr 10.0 * log10([time2real $INIT_RATE_])]
    Animator unset INIT_RATE_
  }
  if [info exists INIT_TIME_] {
    set now [time2real $INIT_TIME_]
    Animator unset INIT_TIME_
  }

  # Initialize peers
  set peerName ""
  set peers ""
  if {$args != ""} {
    catch "array set opts $args"
    if [info exists opts(p)] {
      set peerName $opts(p)
      peer_init $peerName
    }
    if [info exists opts(f)] {
      # User-specified initialization file
      if [file exists $opts(f)] {
        source $opts(f)
      }
    }
    if [info exists opts(j)] {
      # Startup time
      set now [time2real $opts(j)]
    }
    if [info exists opts(r)] {
      # Startup animation rate
      set currRate [expr 10.0 * log10([time2real $opts(r)])]
    }
    if [info exists opts(z)] {
      # Playing nam automatically
      set do_validate_nam 1
    }    
    if [info exists opts(s)] {
      # Playing all windows in sync
      set isSync_ 1
    }
  }

  if [catch {
    $self settime $now
    $self set_rate $currRate 1
  } errMsg] {
    error "$errMsg" 
  }

  if { $showenergy_ == 1 } {
     $self energy_view
  }

  # Hook it to the trace
  $trace connect [new TraceHook $self]

  if { $do_validate_nam == 1 } {
    $self play 1
  }
}

#-----------------------------------------------------------------------
#
#-----------------------------------------------------------------------
Animator instproc infer-network-model { tracefile } {
  #nam_prelayout parses the tracefile before any layout is attempted
  #to see if there is enough header information to use manual layout
  #or whether auto-layout must be performed.
  $self instvar NETWORK_MODEL

  #set file [open $tracefile "r"]
  set stream [new NamStream $tracefile]
  set NETWORK_MODEL NetworkModel

  if { ![regexp "^_" $stream] } {
    puts "\nnam: Unable to open the file \"$tracefile\"\n"
    exit
  }

  # skip all beginning non "*" events
  set time "0"
  while {([$stream eof]==0)&&([string compare $time "*"]!=0)} {
    set line [$stream gets]
    set time [get_trace_item "-t" $line]
  }

  set num_link 0 
  while {([$stream eof]==0)&&([string compare $time "*"]==0)} {
    set cmd [lindex $line 0]
    set time [get_trace_item "-t" $line]
    if {[string compare $time "*"]!=0} {
      break
    }

    # Checking for autolayout (l) and wireless modes (W)
    switch $cmd {
      "l" {
        set direction [get_trace_item "-O" $line]
        if {$direction==""} {
          set direction [get_trace_item "-o" $line]
        }
        if {$direction==""} {
          set NETWORK_MODEL NetworkModel/Auto
        }
        incr num_link
      }
      "W" {
        set NETWORK_MODEL NetworkModel/Wireless
        set width [get_trace_item "-x" $line]
        set height [get_trace_item "-y" $line]
        NetworkModel/Wireless set Wpxmax_ $width
        NetworkModel/Wireless set Wpymax_ $height
        break
      }
    }
    # XXX
    # assuming that every line end with End-OF-Line
    set line [$stream gets]
  }
  $stream close
  if {$num_link == 0 && [string compare $NETWORK_MODEL "NetworkModel/Wireless"] !=0 } {
    # No link
    set NETWORK_MODEL NetworkModel/Auto
  }
}

#-----------------------------------------------------------------------
#
#-----------------------------------------------------------------------
Animator instproc peer_init { name handle } {
	$self peer $name 0 $handle
	$self peer_cmd 0 "peer \"[tk appname]\" 1 $self"
}

Animator instproc peer_cmd { async cmd } {
	$self instvar peers
	foreach s $peers {
		remote_cmd $async [lindex $s 0] "[lindex $s 1] $cmd"
	}
	$self sync_cmd $cmd
}

Animator instproc sync_cmd { cmd } {
	$self tkvar isSync_
	if !$isSync_ {
		return
	}
	Animator instvar SyncAnimator_
	foreach s $SyncAnimator_ {
		if {$s == $self} { continue }
		eval $s $cmd
	}
}

Animator instproc peer { name remote handle } {
	$self instvar peers
	if { $remote } {
		peer_cmd 1 "peer \"$name\" 0 $self"
		foreach s $peers {
			set p [lindex $s 0]
			set h [lindex $s 1]
			remote_cmd 1 $name "peer \"$p\" 0 $h"
		}
	}
	lappend peers [list $name $handle]
}

Animator instproc backFrame { } {
	$self instvar now timeStep
	$self settime [expr $now - $timeStep]
}

Animator instproc nextFrame { } {
  $self instvar now timeStep direction
  if {$direction > 0} {
    $self settime [expr $now + $timeStep]
  } else {
    $self settime [expr $now - $timeStep]
  }
}

Animator instproc stopmaxtime { stoptime } {

	$self instvar maxtimeflag maxtime now mslider pipemode
	if {$pipemode == 0 } { return}

	if {$maxtimeflag == 1} {return}
	
	set maxtimeflag 1
	set maxtime $stoptime
	set now $stoptime
	$mslider setcurrenttime $now
	$mslider setpipemode 0
}

#-----------------------------------------------------------------------
# Animator instproc settime t 
#  - Set time slider to a tick value between 0 and 1000.
#  - t is the current time
#-----------------------------------------------------------------------
Animator instproc settime t {
  $self instvar sliderPressed range mintime timeSlider trace now \
        maxtime graphs netViews statsViews maxtimeflag mslider pipemode
  $self instvar netModel
  $self tkvar nowDisp
  $self tkvar showData_ showRouting_ showMac_

  $netModel select-pkt $showData_  $showRouting_  $showMac_

  # We have come to the end of the animation
  if { $t > $maxtime } {
    if {$pipemode == 0 } {
      $self stop 1
      return
    # Since we are reading from a pipe
    # we must extend the maximum time of the animation
    } else {
      # maxtime has been reached
      if {$maxtimeflag == 1} {
        $self stop 1
        set range [expr $maxtime-$mintime]
        $mslider setmaxtime $maxtime 
        # maxtime window has been found. go back to normal case
        set pipemode 0
        return
      # enlarge the maxtime window
      } else {
        set maxtime [expr $maxtime+10.0]
        set range [expr $maxtime-$mintime]
        $mslider setmaxtime $maxtime 
      }
    }

  # Continue with the animation at this time
  } elseif { $t < $mintime } {
    set t $mintime
  }

  set now $t
  set nowDisp [format %.6f $now]


  # Move timeslider to proper position
  if { $sliderPressed == 0 } {
      $self timesliderset $now
  }

  # tell current animator time to observer
  $self notifyObservers $now

  set event [$trace settime $now $sliderPressed]

  foreach graph $graphs {
      $graph settime $t
  }
  foreach netView $netViews {
      $self update_monitors $netView $now
  }
  # Update positions of annotations
  $self update_annotations $now
}


#-----------------------------------------------------------------------
#
#-----------------------------------------------------------------------
Animator instproc slidetime { tick remote } {
	$self instvar now range mintime trace
	set now [expr ($tick * $range) / 1000. + $mintime]
	$self settime $now
	if { $remote } {
		#$self peer_cmd 1 "slidetime $tick 0"
		# XXX it should do settime instead of slidetime
		$self peer_cmd 1 "settime $now"
	}
}

Animator instproc recordFrame {} {
	# xxx: needs more support for multiple views,
	# and for standard filename endings
	$self instvar netViews nam_record_filename nam_record_frame
	foreach netView $netViews {
		$netView record_frame [format "$nam_record_filename%05d.xwd" $nam_record_frame]
		incr nam_record_frame
	}
}

Animator instproc renderFrame { } {
	$self instvar running direction sliderPressed granularity \
			pending_frame_
	$self tkvar nam_record_animation

	if { $running && !$sliderPressed } {
		$self nextFrame
		update idletasks
		if $nam_record_animation {
			$self recordFrame
		}
		set pending_frame_ [after $granularity "$self renderFrame"]
	}
}

Animator instproc redrawFrame {} {
	$self instvar now
	$self settime $now
}

Animator instproc remote_set_direction {t dir} {
	$self instvar direction
	$self settime $t
	set direction $dir
}

Animator instproc set_forward_dir remote {
	$self instvar direction now 
	set direction 1
	if { $remote } {
		$self peer_cmd 1 "remote_set_direction $now 1"
	}
}

Animator instproc set_backward_dir remote {
	$self instvar direction now
	set direction -1
	if { $remote } {
		$self peer_cmd 1 "remote_set_direction $now -1"
	}
}

Animator instproc remote_play t {
        $self settime $t
        $self play 0
}

Animator instproc play {remote} {
	$self instvar running now
	set running 1
	after 0 "$self renderFrame"
	if { $remote } {
		$self peer_cmd 1 "remote_play $now"
	}
}

Animator instproc remote_stop t {
        $self stop 0
        $self settime $t
}

Animator instproc stop remote {
	$self instvar running now
	set running 0
	if { $remote } {
		$self peer_cmd 1 "remote_stop $now"
	}
}

Animator instproc reset { } {
	$self settime 0.
	$self peer_cmd 1 "settime 0."
}

Animator instproc rewind { } {
	$self instvar now timeStep
	set t [expr $now - $timeStep*25.0]
	$self settime $t
	$self peer_cmd 1 "settime $t"
}

Animator instproc fast_fwd { } {
	$self instvar now timeStep
        set t [expr $now + $timeStep*25.0]
        $self settime $t
	$self peer_cmd 1 "settime $t"
}

Animator instproc next_event { } {
	$self instvar trace running
	set t [$trace nxtevent]
	$self settime $t
	$self peer_cmd 1 "settime $t"
	if { !$running } {
		$self nextFrame
		$self peer_cmd 1 nextFrame
	}
}

Animator instproc set_rate_ext { v remote } {
	$self instvar timeStep stepDisp rateSlider currRate
	set orig_v $v
	set timeStep [time2real $v]
	set v [expr 10.0 * log10($timeStep)]
	set stepDisp [step_format $timeStep]
        if { [$rateSlider get] != $v } { $rateSlider set $v }
	set currRate $v
	if { $remote } {
		$self peer_cmd 1 "set_rate $orig_v 0"
	}
}

Animator instproc set_rate { v remote } {
	$self instvar timeStep stepDisp rateSlider currRate
	set orig_v $v
	set timeStep [expr pow(10, $v / 10.)]
	set stepDisp [step_format $timeStep]
        if { [$rateSlider get] != $v } { $rateSlider set $v }
	set currRate $v
	if { $remote } {
		$self peer_cmd 1 "set_rate $orig_v 0"
	}
}

# set how long broadcast packets are visible
Animator instproc set_bcast_duration { time } {
	$self instvar netModel
	$netModel set bcast_duration_ $time
}
# set the radius of broadcast packets
Animator instproc set_bcast_radius { time } {
	$self instvar netModel
	$netModel set bcast_radius_ $time
}

# Set time to its previous value (before it was changed by
# pressing mouse button 1 on the time slider).
Animator instproc time_undo { } {
        $self instvar timeSlider prevTime now
        set currTime $now
        $self settime $prevTime
	$self peer_cmd 1 "settime $prevTime"
        set prevTime $currTime
}

# Set rate to its previous value (before it was changed by
# pressing mouse button 1 on the rate slider).
Animator instproc rate_undo { } {
        $self instvar prevRate rateSlider
        set tmpRate [$rateSlider get]
        $self set_rate $prevRate 1
        $rateSlider set $prevRate
        set prevRate $tmpRate
}

## following implements auto fast forward:  when nothing is going
## on time is moved forward to next event.

# for now, this proc can be called from the trace file using an 
# annotation to enable auto fast forward
Animator instproc auto_ff { } {
	$self tkvar nam_auto_ff
	set nam_auto_ff 1
}

# skip to next event 
Animator instproc speedup { t } {
	$self instvar running 
	$self tkvar nam_auto_ff
	if { $nam_auto_ff && $running } {
		$self next_event 
	}
}

# do nothing, but could be more interesting when paired with a different
# implementation of speedup.  e.g., speedup could adjust timeStep and
# this proc could restore it.
Animator instproc normalspeed { } {
}

Animator instproc button_release_1 t {
        $self instvar timeSlider sliderPressed

	$self slidetime $t 1
        $timeSlider set $t
	set sliderPressed 0
}

Animator instproc button_press_1 s {
	$self instvar sliderPressed prevTime
	set sliderPressed 1
        set prevTime $s
}

Animator instproc displayStep args {
	$self instvar rateSlider stepDisp
	$rateSlider configure -label "Step: $stepDisp"
}

Animator instproc back_step { } {
        $self instvar running
        if $running { $self stop 1 }
	$self backFrame
	$self peer_cmd 1 backFrame
}

Animator instproc toggle_pause { } {
        $self instvar running
        if $running {
		$self stop 1
	} else {
		$self play 1
	}
}

Animator instproc single_step { } {
        $self instvar running
        if $running { $self stop 1 }
        $self nextFrame
	$self peer_cmd 1 nextFrame
}

Animator instproc dead name {
	$self instvar peers
	set i [lsearch -exact $peers $name]
	set peers [lreplace $peers $i $i]
}

Animator instproc destroy {} {
	[AnimControl instance] close-animator $self
	$self next
}

Animator instproc cleanup {} {
	# We should do all tkvar-related cleanup before doing "delete"
	if [$self is-sync] {
		$self remove-sync $self
	}
	$self instvar pending_frame_ tlw_ balloon_
	# In case there's a balloon hanging around
	delete $balloon_
	destroy $tlw_
	# Cancel pending animation events
	if [info exists pending_frame_] {
		after cancel $pending_frame_
	}
}

Animator instproc done { } {
        $self peer_cmd 1 "dead \"[tk appname]\""
	$self cleanup
	delete $self
}

Animator instproc all_done { } {
	$self peer_cmd 1 "destroy"
	$self cleanup
	delete $self
}

Animator instproc local_change_rate inc {
        $self instvar timeStep stepDisp
        if $inc {
	        set timeStep [expr $timeStep + $timeStep*0.05]
	} else {
	        set timeStep [expr $timeStep - $timeStep*0.05]
	}	    
        set stepDisp [step_format $timeStep]
}

Animator instproc change_rate inc {
	$self instvar timeStep
	$self local_change_rate $inc
	$self peer_cmd 1 "local_change_rate $inc"
}

Animator instproc ncolor {n0 color} {
	$self instvar netModel
	$netModel ncolor $n0 $color
}

Animator instproc ecolor {n0 n1 color} {
	$self instvar netModel
	$netModel ecolor $n0 $n1 $color
	$netModel ecolor $n1 $n0 $color
}

Animator instproc add-sync { anim } {
	Animator instvar SyncAnimator_
	lappend SyncAnimator_ $anim
	$self tkvar isSync_
}

Animator instproc remove-sync { anim } {
	Animator instvar SyncAnimator_
	set pos [lsearch $SyncAnimator_ $anim]
	if {$pos != -1} {
		set SyncAnimator_ [lreplace $SyncAnimator_ $pos $pos]
	}
}

Animator instproc is-sync {} {
	$self tkvar isSync_
	return $isSync_
}

Animator instproc get-sync {} {
	Animator instvar SyncAnimator_
	return $SyncAnimator_
}

Animator instproc get-name {} {
	$self instvar nam_name
	return $nam_name
}

Animator instproc get-tkwin {} {
	$self instvar tlw_
	return $tlw_
}

# XXX better to inherit it from Observable
        
# Adds an observer to the set of observers for this object.
# @param   o   an observer to be added. 

Animator instproc addObserver { o } {
    $self instvar observerlist_

    set cnt 0
    set oid [$o id]
    foreach ob $observerlist_ {
        set obid [$ob id]
        if { $oid == $obid } {
            set cnt 1
            break;
        } 
    }   
    
    if { $cnt == 0 } {
        lappend observerlist_ $o
    }   
}

# Deletes an observer from the set of observers of this object.
# @param   o   the observer to be deleted.
                
Animator instproc  deleteObserver { o } { 

    $self instvar observerlist_
    set backlist_ ""
    set oid [$o id]
    foreach ob $observerlist_ {
        set obid [$ob id]
        if { $oid != $obid } {
            lappend backlist_ $ob
        } else {
            $o destory
        }
    }
    
    set observerlist_ $backlist_
}

#----------------------------------------------------------------------
# If this object has changed, as indicated by the
# hasChanged method, then notify all of its observers
# and then call the clearChanged method to indicate
# that this object has no longer changed.
#
# Each observer has its update method called with two
# arguments: this observable object and the arg argument
#
# - What is an observer?  looks like it is NamgraphModel,
#   NamgraphView, TimesliderNamgraphView, and TimesliderView
#----------------------------------------------------------------------
Animator instproc notifyObservers { arg } {

    $self instvar observerlist_

    #??? Synchronization here before updating ???

    foreach ob $observerlist_ {

        if ![ catch { $ob info class } ] {

            $ob update $arg
        }
    }
}

Animator instproc show-energy {} {
    $self instvar showenergy_
    set showenergy_ 1
}


# Returns the number of observers of this object.

Animator instproc countObservers {} {
    $self instvar observerlist_
    set size [llength $observerlist_]
    return $size
}


