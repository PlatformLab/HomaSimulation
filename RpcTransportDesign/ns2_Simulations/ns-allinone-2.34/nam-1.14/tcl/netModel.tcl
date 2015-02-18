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
# $Header: /cvsroot/nsnam/nam-1/tcl/netModel.tcl,v 1.25 2003/01/28 03:22:39 buchheim Exp $

NetworkModel set bcast_duration_ 0.01
# 250 is pre-calculate by using fixed two-way ground reflection
# model with fixed transmission power 3.652e-10
NetworkModel set bcast_radius_ 250

#-----------------------------------------------------------------------
# NetworkModel instproc init {animator tracefile}
#-----------------------------------------------------------------------
NetworkModel instproc init {animator tracefile} {
  $self instvar showenergy_
  $self next $animator	;# create shadow, etc.

  # Tracefile maybe a space which indicates that
  # a new nam editor window is being created
  if {$tracefile != " "} {
    # We have a tracefile so it must be an animation to display
    $self nam_addressing $tracefile
    $self nam_layout $tracefile
    $self layout

    if [info exists showenergy_] {
      if {$showenergy_ == 1} {
        $animator show-energy
      }
    }
  }
}

# Read events from the trace file and extract addressing information from them
NetworkModel instproc nam_addressing { tracefile } {
    set stream [new NamStream $tracefile]
    set time "0"
    # skip all beginning non "*" events
    while {([$stream eof]==0)&&([string compare $time "*"]!=0)} {
	set line [$stream gets]
	set time [get_trace_item "-t" $line]
    }

    $self instvar netAddress
    if ![info exists netAddress] {
	    set netAddress [new Address]
    }
    while {([$stream eof]==0)&&([string compare $time "*"]==0)} {
	set cmd [lindex $line 0]
	set time [get_trace_item "-t" $line]
	if {[string compare $time "*"]!=0} {break}
	switch $cmd {
		"A" {
			set hier [get_trace_item "-n" $line]
			if {$hier != ""} {
				# PortShift
				set ps [get_trace_item "-p" $line]
				# PortMask
				set pm [get_trace_item "-o" $line]
				# McastShift
				set ms [get_trace_item "-c" $line]
				# McastMask
				set mm [get_trace_item "-a" $line]
				$netAddress portbits-are $ps $pm
				$netAddress mcastbits-are $ms $mm
			} else {
				set hier [get_trace_item "-h" $line]
				# NodeShift
				set nh [get_trace_item "-m" $line]
				# NodeMask
				set nm [get_trace_item "-s" $line]
				$netAddress add-hier $hier $nh $nm
			}
		}
	}
	# XXX
	# assuming that every line end with End-OF-Line
	set line [$stream gets]
    }
    $stream close
}

#-----------------------------------------------------------------------
# NetworkModel instproc nam_layout { tracefile } {
#   - read events from the tracefile and parse them as 
#     layout commands until we meet an event without -t * 
#-----------------------------------------------------------------------
NetworkModel instproc nam_layout { tracefile } {
  set stream [new NamStream $tracefile]
  set time "0"

    # skip all beginning non "*" events
  while {([$stream eof]==0)&&([string compare $time "*"] != 0)} {
    set line [$stream gets]
    set time [get_trace_item "-t" $line]
  }

  while {([$stream eof]==0)&&([string compare $time "*"] == 0)} {
    set cmd [lindex $line 0]
    set time [get_trace_item "-t" $line]
    if {[string compare $time "*"] != 0} {
      break
    }

    switch $cmd {
      "n" {
#        $self layout_node [lrange $line 1 end]
        # goto netmodel.cc: command to add the node
        $self node $line
      }
      "g" {
        $self layout_agent [lrange $line 1 end]
      }
      "l" {
        $self layout_link [lrange $line 1 end]
      }
      "L" {
        $self layout_lanlink [lrange $line 1 end]
      }
      "X" {
        $self layout_lan [lrange $line 1 end]
      }
      "q" {
        $self layout_queue [lrange $line 1 end]
      }
      "c" {
        $self layout_color [lrange $line 1 end]
      }
    }
    # XXX
    # assuming that every line end with End-OF-Line
    set line [$stream gets]
  }
  $stream close
}

#-----------------------------------------------------------------------
# This procedure is no longer in use
#-----------------------------------------------------------------------
NetworkModel instproc layout_node {line} {
  $self instvar showenergy_

  # Setup default values
  set color black
  set lcolor black
  set shape circle
  set dlabel ""
  set src ""
  set size 0
  set addr 0
  
  for {set i 0} {$i < [llength $line]} {incr i} {
    set item [string range [lindex $line $i] 1 end]
    switch $item {
      "s" {
        set src [lindex $line [expr $i+1]]
      }
      "v" {
        set shape [lindex $line [expr $i+1]]
      }
      "c" {
        set color [lindex $line [expr $i+1]]
        if {[string range $color 0 0] == "\#"} {
          set rgb [winfo rgb . $color]
          set color "[$self lookupColorName [lindex $rgb 0] \
                     [lindex $rgb 1] [lindex $rgb 2]]"
        }
        if [info exists showenergy_] {
           if { $color == "green"} {
             set showenergy_ 1
           }
        }
      }
      "z" {
        set size [lindex $line [expr $i+1]]
      }
      "a" {
        set addr [lindex $line [expr $i+1]]
      }
      "x" {
        set xcor [lindex $line [expr $i+1]]
        set showenergy_ 0
      }
      "y" {
        set ycor [lindex $line [expr $i+1]]
        set showenergy_ 0
      }
      "i" {
        set lcolor [lindex $line [expr $i+1]]
        if {[string range $lcolor 0 0] == "\#"} {
          set rgb [winfo rgb . $lcolor]
          set lcolor "[$self lookupColorName [lindex $rgb 0] \
                      [lindex $rgb 1] [lindex $rgb 2]]"
        }
      }
      "b" {
        set dlabel [lindex $line [expr $i+1]]
      }
    }

    if {[string range $item 0 0]=="-"} {
      incr i
    }
  }
  if {$src==""} {
    puts "Error in parsing tracefile line:\n$line\n"
    puts "No -s <node id> was given."
    exit
  }

  # XXX Make sure dlabel is the last argument. This stupid node creation 
  # process should be changed and a node-config method should be used to
  # add dlabels, etc., to nodes. 
  # 
  # XXX TO ALL WHO WILL ADD MORE ARGUMENTS:
  # in netmodel.cc, NetModel::addNode() says: if there are more than 9 
  # arguments (incl. 'cmd node'), this is a wireless node! 
  # You be careful!!
  if [info exists xcor] {
    if {$dlabel != ""} {
      $self node $src $shape $color $addr $size $lcolor \
                 $xcor $ycor $dlabel 
    } else {
      $self node $src $shape $color $addr $size $lcolor \
                 $xcor $ycor
    }
  } else {
    if {$dlabel != ""} {
      $self node $src $shape $color $addr $size $lcolor $dlabel
    } else {
      $self node $src $shape $color $addr $size $lcolor
    }
  }
}

#-----------------------------------------------------------------------
#
#-----------------------------------------------------------------------
NetworkModel instproc layout_agent {line} {
    set label ""
    set role ""
    set node ""
    set flowcolor black
    set winInit ""
    set win ""
    set maxcwnd ""
    set tracevar ""
    set start ""
    set producemore ""
    set stop  ""
    set packetsize ""
    set interval ""
    set number ""
    set partner ""

    for {set i 0} {$i < [llength $line]} {incr i} {
	set item [string range [lindex $line $i] 1 end]
	switch $item {
            "l" {
                    set label [lindex $line [expr $i+1]]
            }
	    "r" {
		    set role [lindex $line [expr $i+1]]
	    }
	    "s" {
			set node [lindex $line [expr $i+1]]
		}
		"c" {
			set flowcolor [lindex $line [expr $i+1]]
		}
		"i" {
			set winInit [lindex $line [expr $i+1]]
		}
		"w" {
			set win [lindex $line [expr $i+1]]
		}
		"m" {
			set maxcwnd [lindex $line [expr $i+1]]
		}
		"t" {
			set tracevar [lindex $line [expr $i+1]]
		}
		"b" {
			set start [lindex $line [expr $i+1]]
		}
	        "o" {
	                set stop [lindex $line [expr $i+1]]
	        }
		"p" {
			set producemore [lindex $line [expr $i+1]]
		}
	        "k" {
	                set packetsize [lindex $line [expr $i+1]]
	        }
	        "v" {
	                set interval [lindex $line [expr $i+1]]
	        }
	        "n" {
	                set number [lindex $line [expr $i+1]]
	        }
	        "u" {
	                set partner [lindex $line [expr $i+1]]
	        }
	}
	if {[string range $item 0 0]=="-"} {incr i}
	}
	if {$label==""} {
	puts "Error in parsing tracefile line:\n$line\n"
	exit
	}

	if {[string compare $role "20"]==0} {
	$self agent $label $role $node $number $partner

	} elseif {[string compare $role "10"]==0 && [string compare $label "CBR"]==0} {
	    $self agent $label $role $node $flowcolor $packetsize $interval $start $stop $number $partner

	} else {
	$self agent $label $role $node $flowcolor $winInit $win $maxcwnd $tracevar $start $producemore $stop $number $partner
	}
}


#-----------------------------------------------------------------------
# NetworkModel instproc layout_lan {line}
#  - creates a virtual node to which other nodes are connect using a
#	shared lan via the 'L' event
#-----------------------------------------------------------------------
NetworkModel instproc layout_lan {line} {
  set name ""
  set rate ""
  set delay ""
  set orient down
  for {set i 0} {$i < [llength $line]} {incr i} {
	set item [string range [lindex $line $i] 1 end]

	switch $item {
	  "n" {
	    set name [lindex $line [expr $i+1]]
	  }
	  "r" {
	    set rate [lindex $line [expr $i+1]]
	  }
	  "D" {
	    set delay [lindex $line [expr $i+1]]
	  }
	  "o" {
	    set orient [lindex $line [expr $i+1]]
	  }
	}

	if {[string range $item 0 0]=="-"} {
	  incr i
	}
  }

  if {$name==""} {
	puts "netModel.tcl found an Error in parsing tracefile line:\n$line\n"
	exit
  }

  set th [nam_angle $orient]

  # Lan rates are shown in Mb/s and delay in ms
  set rate [expr [bw2real $rate] / 1000000.0]
  set delay [expr [time2real $delay] * 1000.0]

  $self lan $name [bw2real $rate] [time2real $delay] [expr $th]
}

NetworkModel instproc layout_link {line} {
	#orient can default because pre-layout will have kicked us into 
	#autolayout mode by this point
	set orient right
	set src ""
	set dst ""
	set rate 10Mb
	set delay 10ms
	set color ""
	set dlabel ""
	set length 0
	for {set i 0} {$i < [llength $line]} {incr i} {
		set item [string range [lindex $line $i] 1 end]
		switch "$item" {
			"s" {
				set src [lindex $line [expr $i+1]]
			}
			"d" {
				set dst [lindex $line [expr $i+1]]
			}
			"r" {
				set rate [lindex $line [expr $i+1]]
			}
			"D" {
				set delay [lindex $line [expr $i+1]]
			}
			"h" {
				set length [lindex $line [expr $i+1]]
			}
			"O" {
				set orient [lindex $line [expr $i+1]]
			}
			"o" {
				set orient [lindex $line [expr $i+1]]
			}
			"c" {
				set color [lindex $line [expr $i+1]]
			}
			"b" {
				set dlabel [lindex $line [expr $i+1]]
			}
		}
		if {[string range $item 0 0]=="-"} {
			incr i
		}
	}

	if {($src=="")||($dst=="")} {
		puts "Error in parsing tracefile line:\n$line\n"
		exit
	}
	set th [nam_angle $orient]
	set rev [expr $th + 1]
	# This is easier than using mod arithmetic
	if { $rev > 2.0 } {
		set rev [expr $rev - 2.0]
	}

	# Lan rates are shown in Mb/s and delay in ms
	set rate [expr [bw2real $rate] / 1000000.0]
	set delay [expr [time2real $delay] * 1000.0]

	#build the reverse link
	$self link $dst $src $rate $delay $length $rev

	#and the forward one
	$self link $src $dst $rate $delay $length $th
	
	if { $color!="" } {
		$self ecolor $src $dst $color 
		$self ecolor $dst $src $color 
	}

	if { $dlabel!="" } {
		$self edlabel $src $dst $dlabel
		$self edlabel $dst $src $dlabel
	}
}

NetworkModel instproc layout_lanlink {line} {
  #orient can default because pre-layout will have kicked us into 
  #autolayout mode by this point
  set orient right
  set src ""
  set dst ""
  for {set i 0} {$i < [llength $line]} {incr i} {
    set item [string range [lindex $line $i] 1 end]
    switch "$item" {
      "s" {
        set src [lindex $line [expr $i+1]]
      }
      "d" {
        set dst [lindex $line [expr $i+1]]
      }
      "O" {
        set orient [lindex $line [expr $i+1]]
      }
      "o" {
        set orient [lindex $line [expr $i+1]]
      }
    }

    if {[string range $item 0 0]=="-"} {
      incr i
    }
  }

  if {($src=="")||($dst=="")} {
    puts "Error in parsing tracefile line no source or destination in lanlink:\n$line\n"
    exit
  }

  set th [nam_angle $orient] ;# global function

  $self lanlink $dst $src $th
}

NetworkModel instproc layout_queue {line} {
	for {set i 0} {$i < [llength $line]} {incr i} {
		set item [string range [lindex $line $i] 1 end]
		switch $item {
			"s" {
				set src [lindex $line [expr $i+1]]
			}
			"d" {
				set dst [lindex $line [expr $i+1]]
			}
			"a" {
				set angle [lindex $line [expr $i+1]]
			}
		}
		if {[string range $item 0 0]=="-"} {incr i}
	}
	$self queue $src $dst $angle
}

NetworkModel instproc layout_color {line} {
    for {set i 0} {$i < [llength $line]} {incr i} {
	set item [string range [lindex $line $i] 1 end]
	switch $item {
	    "i" {
		set ix [lindex $line [expr $i+1]]
	    }
	    "n" {
		set name [lindex $line [expr $i+1]]
		if {[string range $name 0 0] == "\#"} {
                    set rgb [winfo rgb . $name]
                    set name "[$self lookupColorName [lindex $rgb 0] \
                        [lindex $rgb 1] [lindex $rgb 2]]"
		}

	    }
	}
	if {[string range $item 0 0]=="-"} {incr i}
    }
    $self color $ix $name
}

NetworkModel instproc traffic_filter {type} {
	$self select-traffic $type
}

NetworkModel instproc src_filter {src} {
	$self select-src $src
}

NetworkModel instproc dst_filter {dst} {
	$self select-dst $dst
}

NetworkModel instproc fid_filter {fid} {
	$self select-fid $fid
}

