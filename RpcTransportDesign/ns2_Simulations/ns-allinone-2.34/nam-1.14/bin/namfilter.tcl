# Nam filter: Processing ns nam tracefile to produce info for namgraph

set analysis_OK 0
set analysis_ready 0

# color database
set colorname(0) black
set colorname(1) dodgerblue
set colorname(2) cornflowerblue
set colorname(3) blue
set colorname(4) deepskyblue
set colorname(5) steelblue
set colorname(6) navy
set colorname(7) darkolivegreen

set fcolorname(0) red
set fcolorname(1) brown
set fcolorname(2) purple
set fcolorname(3) orange
set fcolorname(4) chocolate
set fcolorname(5) salmon
set fcolorname(6) greenyellow
set fcolorname(7) gold
set fcolorname(8) red
set fcolorname(9) brown
set fcolorname(10) purple
set fcolorname(11) orange
set fcolorname(12) chocolate
set fcolorname(13) salmon
set fcolorname(14) greenyellow
set fcolorname(15) gold

# get trace item
proc get_trace_item { tag traceevent} {
    set next 0
    foreach item $traceevent {
        if { $next == 1 } {
            return $item
        }   
        if { $item == $tag } {
            set next 1
        }
    }   
    return ""
}

# process version info
proc handle_version { line } {

    global analysis_OK nam_version analysis_ready
    
    set nam_version [get_trace_item "-v" $line]
    if { $nam_version >= "1.0a5" } {
        set analysis_OK 1
    }
    set analysis_ready [get_trace_item "-a" $line]
}

# preanalysis
# return
#	0:	no filter can be applied
#	1:	filters have been applied
#	2:	Applying filters

proc nam_analysis { tracefile } {
    global analysis_OK nam_version analysis_ready

    set file [open $tracefile "r"]
    set line [gets $file]
    set time [get_trace_item "-t" $line]

    # skip all beginning non "*" events
    while {([eof $file]==0)&&([string compare $time "*"]!=0)} {
	set line [gets $file]
	set time [get_trace_item "-t" $line]
    }

    while {([eof $file]==0)&&([string compare $time "*"]==0) } {
        set cmd [lindex $line 0]
        switch "$cmd" {
            "V" {
                 handle_version $line
                 break
            }
        }
        set line [gets $file]
        set time [get_trace_item "-t" $line]
    }
    close $file

    # old nam, skip it
    if { $analysis_OK == 0 } {
         puts "You are using the tracefile format older than 1.0b5"
         puts "which will not allow you to run namgraph"
         return 0
    }

    # check if analysis ready
    if { $analysis_ready == 1 } {
	puts "Filters have been applied to the tracefile before"
	return 1

    } else {
 	return 2	
    }
}

proc nam_prefilter { tracefile } {

    # it only supports tcp & srm so far

    global colorname colorindex session_id highest_seq colorset groupmember 
    global proto_id


    set file [open $tracefile "r"]
    set file2 [open $tracefile.tmp "w"]
    set colorindex 0

    # set color value
    while {[eof $file]==0} {
        set line [gets $file]
        set time [get_trace_item "-t" $line]
	set prot [get_trace_item "-p" $line]
    
        if { [string compare $time "*"]==0 } {
	    puts $file2 $line
	    continue
        }
    
        # get extended info after -x
        set extinfo [get_trace_item "-x" $line]
    
        if {$extinfo==""} {
	    puts $file2 $line
	    continue
 	}
    
        # find the biggest tcp sequence number for the specific session
        #set tmp_seq [lindex $extinfo 2]
        #if { ($highest_seq < $tmp_seq) && ([string compare $prot "tcp"] == 0 \
	#    || [string compare $prot "ack"] == 0) } {
        #    set highest_seq $tmp_seq
        #}

        set src  [lindex $extinfo 0]
        set dst  [lindex $extinfo 1]

	set subtype [lindex $extinfo 4]  
	set subtype_s [split $subtype "_"]
	set srm_flag [lindex $subtype_s 0]

	# tcp packet

        if {(![info exists colorset($src.$dst)]) && (![info exists colorset($dst.$src)]) \
	    && ([string compare $prot "tcp"] == 0 || [string compare $prot "ack"] == 0) } {

            set colorset($src.$dst) $colorindex
            set colorset($dst.$src) $colorindex
    
            set protocol [get_trace_item "-p" $line]

            set session_id($colorindex) "TCP session \
                between node $src and node $dst"
	    set proto_id($colorindex) "TCP"

            incr colorindex
	}

  	# SRM packet

	if { [string compare $srm_flag "SRM"] == 0 } {

	    if { ![info exists colorset($dst)] } {
	        set colorset($dst) $colorindex
		set groupmember($dst) ""
		set session_id($colorindex) SRM-$dst
	        incr colorindex
	    }

	    set matchflag 0

	    foreach member $groupmember($dst) {
	 	if { [string compare $member $src] == 0 } {
		    set matchflag 1
		}
	    }

	    if { $matchflag == 0 } {

		lappend groupmember($dst) $src
	    }
		
	}

	#set specific color

        set attrindex [lsearch $line -a]
        incr attrindex

 	if { [string compare $srm_flag "SRM"] == 0 } {
            set line2 [lreplace $line $attrindex $attrindex $colorset($dst)]
            set line "$line2 -S $colorset($dst)"
	} else {
	    if { [string compare $prot "tcp"] == 0 || \
		[string compare $prot "ack"] == 0 } {
                set line2 [lreplace $line $attrindex $attrindex $colorset($src.$dst)]
                set line "$line2 -S $colorset($src.$dst)"
	    }
	}

   	puts $file2 $line 
    }

    close $file
    close $file2
    exec mv $tracefile.tmp $tracefile 

#    set fcnt $colorindex

}

proc nam_filter { tracefile } {
    global filters filter_num colorindex colorname mcnt

    # set color value in the tracefile
    set tracefilebackup $tracefile.tmp    
    set file [open $tracefile "r"]
    set file2 [open $tracefilebackup "w"]
    
    # define color value in the tracefile
    
    #for {set i 0} {$i < $colorindex} {incr i} {
    #    puts $file2 "c -t * -i $i -n $colorname($i)"
    #}
    
    while {[eof $file]==0} {
    
        set line [gets $file]
        set cmd [lindex $line 0]
    
        if {$line==""} {continue}

	# Apply filters here

	for {set i 0} {$i < $filter_num} {incr i} {
	    set filtercmd [list $filters($i) $line]
	    set line [eval $filtercmd] 
	# puts $file2 $result
        }
 
	puts $file2 $line	

    }

    close $file
    close $file2
    
    exec mv $tracefile.tmp $tracefile
}

# Filter srm packet

proc srm_filter {event} {
    global colorset fname fcnt mcnt colorindex srmdata_s srmsess_s srmrqst_s \
	srmrepr_s srmdata_r srmsess_r srmrqst_r srmrepr_r srmrqst_rf srmrepr_rf \
	srmrepr_sf srmrqst_sf groupmember

    set extinfo [get_trace_item "-x" $event]
    set type [lindex $event 0]

    if {$extinfo!=""} {

        #get srm flags
        set flagitem [lindex $extinfo 4]

        #filter tcp packet

        set protocol [get_trace_item "-p" $event]
        set cmd  [lindex $event 0]
    
        #1. SRM_DATA 
        #2. SRM_SESS
        #3. SRM_RQST
        #4. SRM_REPR
    
        if {[string compare $flagitem "SRM_DATA"] == 0 } {

	    set src [get_trace_item "-s" $event]
	    set ssrc [lindex $extinfo 0]
 	    set ssrcs [split $ssrc "."]
	    set ssrc0 [lindex $ssrcs 0]
	
	    # sending SRM_DATA from the source
	    
	    if { [string compare $src $ssrc0 ] == 0  && \
		[string compare $type "h" ] == 0} {

                if { ![info exists srmdata_s] } {
                    set srmdata_s $mcnt
                    set fname($mcnt) "SRM_DATA_SEND"
                    incr mcnt
                }
        
                set fbit [get_trace_item "-f" $event]
		set sid [get_trace_item "-S" $event]

                if { $fbit == "" } {
                    set event "$event -f $sid -m $srmdata_s"
                } 
            }

	    set ddst [lindex $extinfo 1]
	    set dst [get_trace_item "-d" $event]
	    set cnt 0
	    
            foreach value $groupmember($ddst) {
            	set ddsts [split $value "."]
            	set ddst0 [lindex $ddsts 0]
	
		if { [string compare $ddst0 $dst] == 0 } {
		    set cnt 1
		    break
		}
            }
 
	    if { $cnt == 1 && [string compare $type "r" ] == 0 } {
		if { ![info exists srmdata_r] } {
		    set srmdata_r $mcnt
                    set fname($mcnt) "SRM_DATA_RECV"
                    incr mcnt
                }

                set fbit [get_trace_item "-f" $event]
                set sid [get_trace_item "-S" $event]

                if { $fbit == "" } {
                    set event "$event -f $sid -m $srmdata_r"
                }
            }

        }     

	# SRM_SESSION

        if {[string compare $flagitem "SRM_SESS"] == 0 } {

	    set ddst [lindex $extinfo 1]
            set src [get_trace_item "-s" $event]
            set cnt 0

            foreach value $groupmember($ddst) {
                set ddsts [split $value "."]
                set ddst0 [lindex $ddsts 0]

                if { [string compare $ddst0 $src] == 0 } {
                    set cnt 1
                    break
                }
            }

        
            # sending SESSION from the source

            if { $cnt == 1 && [string compare $type "h" ] == 0 } {

                if { ![info exists srmsess_s] } {
                    set srmsess_s $mcnt
                    set fname($mcnt) "SRM_SESS_SEND"
                    incr mcnt
                }

                set fbit [get_trace_item "-f" $event]
                set sid [get_trace_item "-S" $event]

                if { $fbit == "" } { 
                    set event "$event -f $sid -m $srmsess_s"
                }
            }   

            set ddst [lindex $extinfo 1]
            set dst [get_trace_item "-d" $event]
            set cnt 0

            foreach value $groupmember($ddst) {
                set ddsts [split $value "."]
                set ddst0 [lindex $ddsts 0]

                if { [string compare $ddst0 $dst] == 0 } {
                    set cnt 1
                    break
                }
            }

            if { $cnt == 1 && [string compare $type "r" ] == 0 } {
                if { ![info exists srmsess_r] } {
                    set srmsess_r $mcnt
                    set fname($mcnt) "SRM_SESS_RECV"
                    incr mcnt
                }

                set fbit [get_trace_item "-f" $event]
                set sid [get_trace_item "-S" $event]

                if { $fbit == "" } {
                    set event "$event -f $sid -m $srmsess_r"
                }
            }


        }


        # RQST

        if {[string compare $flagitem "SRM_RQST"] == 0 } {

            set ddst [lindex $extinfo 1]
            set src [get_trace_item "-s" $event]
            set cnt 0

            foreach value $groupmember($ddst) {
                set ddsts [split $value "."]
                set ddst0 [lindex $ddsts 0]

                if { [string compare $ddst0 $src] == 0 } {
                    set cnt 1
                    break
                }
            }


            if { $cnt == 1 && [string compare $type "h" ] == 0 } {

                if { ![info exists srmrqst_s] } {
                    set srmrqst_s $mcnt
		    set srmrqst_sf $fcnt
                    set fname($mcnt.$fcnt) "SRM_RQST_SEND"
                    incr mcnt
		    incr fcnt
                }

                set fbit [get_trace_item "-f" $event]
                set sid [get_trace_item "-S" $event]

                if { $fbit == "" } {
                    set event "$event -f $srmrqst_sf -m $srmrqst_s"
                }
            }  

            set ddst [lindex $extinfo 1]
            set dst [get_trace_item "-d" $event]
            set cnt 0

            foreach value $groupmember($ddst) {
                set ddsts [split $value "."]
                set ddst0 [lindex $ddsts 0]

                if { [string compare $ddst0 $dst] == 0 } {
                    set cnt 1
                    break
                }
            }

            if { $cnt == 1 && [string compare $type "r" ] == 0 } {
                if { ![info exists srmrqst_r] } {
		    set srmrqst_rf $fcnt
                    set srmrqst_r $mcnt
                    set fname($mcnt.$fcnt) "SRM_RQST_RECV"
                    incr mcnt
		    incr fcnt
                }

                set fbit [get_trace_item "-f" $event]
                set sid [get_trace_item "-S" $event]

                if { $fbit == "" } {
                    set event "$event -f $srmrqst_rf -m $srmrqst_r"
                }
            }

        }

	# REPR

        if {[string compare $flagitem "SRM_REPR"] == 0 } {

            set ddst [lindex $extinfo 1]
            set src [get_trace_item "-s" $event]
            set cnt 0

            foreach value $groupmember($ddst) {
                set ddsts [split $value "."]
                set ddst0 [lindex $ddsts 0]

                if { [string compare $ddst0 $src] == 0 } {
                    set cnt 1
                    break
                }
            }


            if { $cnt == 1 && [string compare $type "h" ] == 0 } {

                if { ![info exists srmrepr_s] } {
                    set srmrepr_s $mcnt
                    set srmrepr_sf $fcnt
                    set fname($mcnt.$fcnt) "SRM_REPR_SEND"
                    incr mcnt
                    incr fcnt
                }

                set fbit [get_trace_item "-f" $event]
                set sid [get_trace_item "-S" $event]

                if { $fbit == "" } {
                    set event "$event -f $srmrepr_sf -m $srmrepr_s"
                }
            }

            set ddst [lindex $extinfo 1]
            set dst [get_trace_item "-d" $event]
            set cnt 0

            foreach value $groupmember($ddst) {
                set ddsts [split $value "."]
                set ddst0 [lindex $ddsts 0]

                if { [string compare $ddst0 $dst] == 0 } {
                    set cnt 1
                    break
                }
            }

            if { $cnt == 1 && [string compare $type "r" ] == 0 } {
                if { ![info exists srmrepr_r] } {
                    set srmrepr_rf $fcnt
                    set srmrepr_r $mcnt
                    set fname($mcnt.$fcnt) "SRM_REPR_RECV"
                    incr mcnt
                    incr fcnt 
                }

                set fbit [get_trace_item "-f" $event]
                set sid [get_trace_item "-S" $event]

                if { $fbit == "" } {
                    set event "$event -f $srmrepr_rf -m $srmrepr_r"
                }
            }

        }

    }

    return $event

}

# Filter tcp packet
proc tcp_filter { event } {

    global colorset fname fcnt mcnt highest_seq tcpmark

    set extinfo [get_trace_item "-x" $event]

    if {$extinfo!=""} {

	#get tcp flags
	set flagitem [lindex $extinfo 3]
	set tcpflags [split $flagitem "-"]

 	#filter tcp packet

	set protocol [get_trace_item "-p" $event]
	set cmd  [lindex $event 0]

        set src  [lindex $extinfo 0]
        set dst  [lindex $extinfo 1]
 
        set lsrc [get_trace_item "-s" $event]
        set ldst [get_trace_item "-d" $event]
        set rsrc [lindex [split $src .] 0]
        set rdst [lindex [split $dst .] 0]
	set sid [get_trace_item "-S" $event]


        if {([string compare $protocol "tcp"] == 0) && \
            ([string compare $cmd "+"] == 0) \
	    && ([string compare $lsrc $rsrc] == 0) } { 

	      if {![info exists tcpmark]} {
	          set tcpmark $mcnt
	          set fname($mcnt) "tcp"
	          incr mcnt
	      }

	      set event "$event -f $sid -m $tcpmark"
	      set sid [get_trace_item "-S" $event]
	      set seqno [lindex $extinfo 2]
	      if {![info exists highest_seq($sid)]} {
		      set highest_seq($sid) $seqno
	      }

	      if { $seqno > $highest_seq($sid) } {
		  set highest_seq($sid) $seqno
	      }

 	}

    }

    return $event

}

#Filter ack packet

proc ack_filter { event } {

    global colorset fname fcnt mcnt ackmark

    set extinfo [get_trace_item "-x" $event]
   
    if {$extinfo!=""} {

        #get tcp flags
        set flagitem [lindex $extinfo 3]
        set tcpflags [split $flagitem "-"]
 
        set is_ecn [lsearch $tcpflags "E"]
        set is_echo [lsearch $tcpflags "C"]

        #filter ack packet 

        set protocol [get_trace_item "-p" $event]
        set cmd  [lindex $event 0]

        set src  [lindex $extinfo 0]
        set dst  [lindex $extinfo 1]
    
        set lsrc [get_trace_item "-s" $event]
        set ldst [get_trace_item "-d" $event]
        set rsrc [lindex [split $src .] 0]
        set rdst [lindex [split $dst .] 0]

	set sid [get_trace_item "-S" $event]

        if {([string compare $protocol "ack"] == 0) && \
            ([string compare $cmd "-"] == 0) && \
	    ([string compare $ldst $rdst] == 0)} { 
            if {![info exists ackmark]} {
                set ackmark $mcnt
 	        set fname($mcnt) "ack" 
                incr mcnt
            }
            set event "$event -f $sid -m $ackmark"
        }   
    }
    return $event

}

#ecn filter

proc ecn_filter { event } {

    global colorset fname fcnt tcpecn tcpcong ackecho ackecn colorindex mcnt \
	   tcpecn_m tcpcong_m ackecho_m ackecn_m mcnt

    set extinfo [get_trace_item "-x" $event]

    if {$extinfo!=""} {

        #get tcp flags
        set flagitem [lindex $extinfo 3]
        set tcpflags [split $flagitem "-"]

        set is_ecn [lsearch $tcpflags "E"]
        set is_cong [lsearch $tcpflags "A"]
        set is_echo [lsearch $tcpflags "C"]

        #filter tcp packet

        set protocol [get_trace_item "-p" $event]
        set cmd  [lindex $event 0]

	#1. tcp ecn
	#2. tcp cong
	#3. ack echo
	#4. ack ecn

        if {([string compare $protocol "tcp"] == 0) && \
	    ($is_ecn != -1)} {
 
            if { ![info exists tcpecn] } {
                set tcpecn $fcnt
		set tcpecn_m $mcnt
                set fname($mcnt.$fcnt) "tcp_ecn"
                incr fcnt
		incr mcnt
            }

            set fbit [get_trace_item "-f" $event]
 	    
	    if { $fbit == "" } {
	        set event "$event -f $tcpecn -m $tcpecn_m"
	    } else {

		set findex [lsearch $event -f]
	  	incr findex
		set line2 [lreplace $event $findex $findex $tcpecn]
		set event $line2

		set mindex [lsearch $event -m]
		incr mindex 
		set line2 [lreplace $event $mindex $mindex $tcpecn_m]
		set event $line2

	    }
 	}

	if {([string compare $protocol "tcp"] == 0) && \
            ($is_cong != -1)} {
 
            if { ![info exists tcpcong] } {   
                set tcpcong $fcnt
		set tcpcong_m $mcnt
                set fname($mcnt.$fcnt) "tcp_cong"
                incr fcnt
		incr mcnt
            }
            
            set fbit [get_trace_item "-f" $event]
            
            if { $fbit == "" } {
                set event "$event -f $tcpcong"
            } else {
                
                set findex [lsearch $event -f]
                incr findex
                set line2 [lreplace $event $findex $findex $tcpcong]
                set event $line2

                set mindex [lsearch $event -m]
                incr mindex 
                set line2 [lreplace $event $mindex $mindex $tcpcong_m]
                set event $line2
            
            }
        }

	if {([string compare $protocol "ack"] == 0) && \
            ($is_echo != -1)} {
 
            if { ![info exists ackecho] } {   
                set ackecho $fcnt
		set ackecho_m $mcnt
                set fname($mcnt.$fcnt) "ack_echo"
                incr fcnt
		incr mcnt
            }
            
            set fbit [get_trace_item "-f" $event]
            
            if { $fbit == "" } {
                set event "$event -f $ackecho"
            } else {
                
                set findex [lsearch $event -f]
                incr findex
                set line2 [lreplace $event $findex $findex $ackecho]
                set event $line2

                set mindex [lsearch $event -m]
                incr mindex 
                set line2 [lreplace $event $mindex $mindex $ackecho_m]
                set event $line2

            
            }
        }

	if {([string compare $protocol "ack"] == 0) && \
            ($is_ecn != -1)} {
 
            if { ![info exists ackecn] } {   
                set ackecn $fcnt
		set ackecn_m $mcnt
                set fname($mcnt.$fcnt) "ack_ecn"
                incr fcnt
		incr mcnt
            }
            
            set fbit [get_trace_item "-f" $event]
            
            if { $fbit == "" } {
                set event "$event -f $tcpecn"
            } else {
                
                set findex [lsearch $event -f]
                incr findex
                set line2 [lreplace $event $findex $findex $ackecn]
                set event $line2

                set mindex [lsearch $event -m]
                incr mindex 
                set line2 [lreplace $event $mindex $mindex $ackecn_m]
                set event $line2

            
            }
        }
    }
    return $event
}

proc nam_afterfilter { tracefile } {

    global colorindex colorname nam_version session_id highest_seq fcolorname \
	   fcnt fname groupmember mcnt proto_id
	   
    set tracefilebackup $tracefile.tmp
    set file [open $tracefile "r"]
    set file2 [open $tracefilebackup "w"]

    #set SRM session info
    for {set i 0} {$i < $colorindex} {incr i} {

        set items [split $session_id($i) "-"] 
	set item1 [lindex $items 0]
#	set item1 [lindex $session_id($i) 0 ]
	if { [string compare $item1 "SRM" ] == 0 } {
	    set item2 [lindex $items 1]
	    set session_id($i) "SRM session with \
	    members: $groupmember($item2)"
	    set proto_id($i) "SRM"
	    set memberlist($i) $groupmember($item2)

	    set cnt 1
	    foreach value $groupmember($item2) {
		set groupindex($i.$value) $cnt
		lappend groupm($i) $value
		incr cnt
	    }
	    set highest_seq($i) $cnt
	    
        }
    }

    #Write nam version info first

    #set realf [expr $fcnt-$colorindex]

    puts $file2 "V -t * -v $nam_version -a 1 -c $colorindex -F $fcnt -M $mcnt"

    # Write color info

    for {set i 0} {$i < $colorindex} {incr i} {
        puts $file2 "c -t * -i $i -n $colorname($i)"
    }
    # write color info for filters

    for {set i $colorindex } {$i < $fcnt} {incr i} {
        set fid [expr $i-$colorindex]
	puts $file2 "c -t * -i $i -n $fcolorname($fid)"
    }

    # Write session info

    for {set i 0} {$i < $colorindex} {incr i} {
#  	puts $file2 "N -t * -S $i -n \{$session_id($i)\}"
	if ![ info exists memberlist($i) ] {

	    set memberlist($i) ""

	}
	puts $file2 "N -t * -S $i -n \{$session_id($i)\} -p $proto_id($i) \
	     -m \{$memberlist($i)\}"

    }

    # Write y mark if necessary

    for {set i 0} {$i < $colorindex} {incr i} {
	if { [info exists groupm($i)] } {
	    for {set j 0} { $j < [llength $groupm($i)]} {incr j} {
	        puts $file2 "N -t * -S $i -m [lindex $groupm($i) $j]"
	    }
	}
    }

    # Write highest y value for each session

    for {set i 0} {$i < $colorindex} {incr i} {
        puts $file2 "N -t * -S $i -h $highest_seq($i)"
    }

    # Write Filter info
    foreach index [array names fname] {
	set fids [split $index "."]
	set fid [lindex $fids 1]
	set mid [lindex $fids 0]

	if { $fid == ""} {
	    puts $file2 "N -t * -F 0 -M $mid -n $fname($index)"
	} else {
	    puts $file2 "N -t * -F $fid -M $mid -n $fname($index)"
	}
    }

    while {[eof $file]==0} {

        set line [gets $file]
        set cmd [lindex $line 0]

        #ignore the original color value & version info
        if { [string compare $cmd "c"]==0 } {continue}
	if { [string compare $cmd "V"]==0 } {continue}
        if {$line==""} {continue} 

	set timev [get_trace_item "-t" $line]
	if {[string compare $timev "*"]==0 } {
	    puts $file2 $line
	    continue
	}

	# color match

	set fv [get_trace_item "-f" $line]
	if { $fv > 1 } {

            set attrindex [lsearch $line -a]
            incr attrindex
 
            set line2 [lreplace $line $attrindex $attrindex $fv]
 
            set line $line2
	}

	# group setting
	set extinfo [get_trace_item "-x" $line]
	set srmflag [lindex $extinfo 4]
	set srmvalues [split $srmflag "_"]
	set srmvalue [lindex $srmvalues 0]
	if { [string compare $srmvalue "SRM"] == 0 } {

	    set sid [get_trace_item "-S" $line]
	    set src [lindex $extinfo 0]
	    set newseq $groupindex($sid.$src)
	    set newindex [lreplace $extinfo 2 2 $newseq]
 	    set extindex [lsearch $line -x]
	    incr extindex
	    set line2 [lreplace $line $extindex $extindex $newindex]
	    set line $line2
	}

	#  define y val in nam graph


	set sess_id [get_trace_item "-S" $line]

	if [info exists proto_id($sess_id)] {

	    set extinfo [get_trace_item "-x" $line]
	    set yval [lindex $extinfo 2]

	    switch -exact -- $proto_id($sess_id) {
	        TCP {

		    set ymark $yval		
	        }
	        SRM {
		    set ymark [lindex $extinfo 0]
	        }
	        default {
		    puts "Unknown protocol event found!"
		    set ymark $yval
	       }
	    }

	    set line "$line -y {$yval $ymark}"

	}

	#filter out -x 

  	set extinfo [get_trace_item "-x" $line]
	if { $extinfo != "" } {
	    set xindex [lsearch $line -x]
	    incr xindex
	    set line2 [lreplace $line $xindex $xindex]
	    set line $line2

	    set xindex [expr $xindex-1]
	    set line2 [lreplace $line $xindex $xindex]
	    set line $line2
	}

	puts $file2 $line
    }
    close $file
    close $file2

    exec mv $tracefile.tmp $tracefile
}

#---------------------------------------------
#main starts here

if { $argc < 1 } {
    puts "Usage: namfilter tracefile-name"
    exit
}

set tracefile [lindex $argv 0]

# save filter name
# CUSTOMIZED PLACE START

set filters(0) tcp_filter
set filters(1) ack_filter
set filters(2) ecn_filter
set filters(3) srm_filter

set filter_num 4

# END OF CUSTOMIZED 

set fcnt 0
set mcnt 0

global fname

if [catch { open $tracefile r } file] {
    puts stderr "Cannot open $tracefile: $fileId"
    exit
} 

close $file

set filtering_flag [nam_analysis $tracefile]
if { $filtering_flag != 2 } {exit}

# Decide how many flows (TCP only so far)
nam_prefilter $tracefile

set fcnt [expr $fcnt+$colorindex]

# Applying filters
nam_filter $tracefile

#Add control info to the head of the tracefile
nam_afterfilter $tracefile

puts "Filters have been applied to the tracefile: $tracefile"

exit

#---- end of the filter --- #
