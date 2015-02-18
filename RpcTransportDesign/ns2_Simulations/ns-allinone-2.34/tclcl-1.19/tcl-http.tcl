#
# Copyright (c) 1998 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#       This product includes software developed by the Computer Systems
#       Engineering Group at Lawrence Berkeley Laboratory.
# 4. Neither the name of the University nor of the Laboratory may be used
#    to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# ------------
#
# Filename: tcl-http.tcl
#   -- Created on Sun May 31 1998
#   -- Author: Cynthia Romer <cromer@cs.berkeley.edu>
#
#  @(#) $Header: /cvsroot/otcl-tclcl/tclcl/tcl-http.tcl,v 1.4 1998/11/17 18:37:58 yatin Exp $
#

#
# Copyright (c) 1998 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#       This product includes software developed by the Computer Systems
#       Engineering Group at Lawrence Berkeley Laboratory.
# 4. Neither the name of the University nor of the Laboratory may be used
#    to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# ------------
#
# Filename: tcl-http.tcl
#   -- Created on Sat May 30 1998
#   -- Author: Yatin Chawathe <yatin@cs.berkeley.edu>
#
# Description:
#
#
#  @(#) $Header: /cvsroot/otcl-tclcl/tclcl/tcl-http.tcl,v 1.4 1998/11/17 18:37:58 yatin Exp $
#



Class HTTP

HTTP public init { } {
	$self next

	# enable the user interface by default only if we are using Tk
	if { [lsearch -exact [package names] Tk] != -1 } {
		$self set enable_output_ 1
	} else {
		$self set enable_output_ 0
	}
	$self set token_count_ 0
}


#
# temporary helper function until Cindy converts the serial geturl requests
# to a sequence of parallel requests
#
HTTP public geturl { url } {
	set token [$self start_fetch $url]
	$self wait
	return $token
}

HTTP public geturls { args } {
	set tokens [eval "$self start_fetch $args"] 
	$self wait
	return $tokens
}

#
# args can be a list of urls
#
HTTP public start_fetch { args } {
	$self instvar token_count_ urls_

	set urls_ $args
	foreach url $args {
		lappend tokens [::http::geturl $url \
				-progress  "$self progress_callback" \
				-command "$self fetch_done"]
		incr token_count_
	}

	if { [llength $tokens] == 1 } {
		return [lindex $tokens 0]
	} else {
		return $tokens
	}
}


HTTP public wait { } {
	$self tkvar vwait_
	if { ![info exists vwait_] } {
		$self start_ping_pong
		# start_ping_pong did an update (during which, the vwait_
		# variable might have been set), so check for existence of
		# vwait_ again
		if { ![info exists vwait_] } { vwait [$self tkvarname vwait_] }
		$self stop_ping_pong
	}
	unset vwait_
}


HTTP private fetch_done { token } {
	$self instvar token_count_
	$self tkvar vwait_
	incr token_count_ -1
	if { $token_count_ <= 0 } {
		set vwait_ 1
		set token_count_ 0
		$self instvar total_bytes_ current_bytes_ per_token_
		unset total_bytes_ current_bytes_ per_token_
	}
}


HTTP private progress_callback { token total_bytes current_bytes } {
	$self instvar total_bytes_ current_bytes_ per_token_
	if { ![info exists total_bytes_] } {
		set total_bytes_ 0
		set current_bytes_ 0
	}

	if { ![info exists per_token_($token)] } {
		set per_token_($token) $current_bytes
		incr total_bytes_ $total_bytes
		incr current_bytes_ $current_bytes
	} else {
		set current_bytes_ [expr $current_bytes_ - \
				$per_token_($token) + $current_bytes]
		set per_token_($token) $current_bytes
	}

	$self instvar urls_
	$self print_status "Fetching $urls_ ... (rcvd $current_bytes_ bytes)"
}


HTTP private build_widget { } {
	$self instvar frame_ rect_
	if { ![info exists frame_] } {
		set cnt 0
		while [winfo exists .http_$cnt] { incr $cnt }
		set frame_ .http_$cnt
		toplevel $frame_
		wm withdraw $frame_
		wm transient $frame_ .
		wm title $frame_ "HTTP Status"
		set new_toplevel 1
	}

	set textheight [font metric http_font -linespace]
	label $frame_.label -font http_font -width 100 \
			-justify left -anchor w -text ""
	canvas $frame_.canvas -height $textheight \
			-width 50 -bd 1 -relief sunken
	pack $frame_.canvas -side right
	pack $frame_.label -expand 1 -fill x -side left

	set rect_ [$frame_.canvas create rectangle 1 2 10 $textheight \
			-fill blue -outline blue]
	$frame_.canvas move $rect_ -1000 0
	
	if [info exists new_toplevel] {
		# center the window
		update idletasks
		update
		set x [expr [winfo screenwidth $frame_]/2 \
				- [winfo reqwidth $frame_]/2 \
				- [winfo vrootx [winfo parent $frame_]]]
		set y [expr [winfo screenheight $frame_]/2 \
				- [winfo reqheight $frame_]/2 \
				- [winfo vrooty [winfo parent $frame_]]]
		wm geom $frame_ +$x+$y
	}       
}


HTTP private start_ping_pong { } {
	if { ![$self set enable_output_] } return

	$self instvar frame_ rect_ after_id_ hide_id_ dir_ pos_
	if { [lsearch -exact [package names] Tk] != -1 } {
		# found Tk; display a UI
		
		if { ![info exists frame_] } {
			$self build_widget
		}
		
		if { [wm state [winfo toplevel $frame_]] == "withdrawn" } {
			wm deiconify $frame_
		}

		if { ![info exists dir_] } {
			set dir_ 2
			set pos_ 1
		}
		
		set coords [$frame_.canvas coords $rect_]
		set x1 [lindex $coords 0]
		set y1 [lindex $coords 1]
		set x2 [lindex $coords 2]
		set y2 [lindex $coords 3]
		
		$frame_.canvas coords $rect_ $pos_ $y1 [expr $pos_-$x1+$x2] $y2
	} else {
		$self set ping_cnt_ 0
		puts -nonewline stderr "Fetching URL "
	}
	set after_id_ [after 100 "$self do_ping_pong"]
	if [info exists hide_id_] { after cancel $hide_id_ }

	$self instvar urls_
	$self print_status "Fetching $urls_ ..."
}


HTTP private stop_ping_pong { } {
	if { ![$self set enable_output_] } return
	
	$self instvar after_id_ hide_id_ frame_ rect_
	if [info exists after_id_] {
		after cancel $after_id_
		unset after_id_
		if { [lsearch -exact [package names] Tk] != -1 } {
			set hide_id_ [after idle "$self hide"]
		} else {
			puts stderr ""
		}
	}
}


HTTP private hide { } {
	$self instvar frame_ rect_ pos_ dir_

	set pos_ 1
	set dir_ 2

	$self print_status ""
	$frame_.canvas move $rect_ -1000 0
	if { [winfo toplevel $frame_] == $frame_ } {
		wm withdraw $frame_
	}
}


HTTP private do_ping_pong { } {
	if { ![$self set enable_output_] } return
	
	$self instvar frame_ rect_ dir_ pos_ after_id_
	if { [lsearch -exact [package names] Tk] != -1 } {      
		incr pos_ $dir_
		$frame_.canvas move $rect_ $dir_ 0
		if { $pos_ <= 1 || $pos_ >= 42 } {
			set dir_ [expr 0 - $dir_]
		}
	} else {
		$self instvar ping_cnt_
		incr ping_cnt_
		if { $ping_cnt_ >= 10 } {
			puts -nonewline stderr "."
			set ping_cnt_ 0
		}
	}
	set after_id_ [after 100 "$self do_ping_pong"]
}


HTTP private print_status { status } {
	if { ![$self set enable_output_] } return
	
	if { [lsearch -exact [package names] Tk] != -1 } {
		$self instvar frame_
		if [info exists frame_] {
			$frame_.label configure -text $status
		}
	}
}


HTTP public set_frame { frame } {
	if { [lsearch -exact [package names] Tk] != -1 } {
		$self instvar frame_
		if [info exists frame_] {
			destroy $frame_
		}
		set frame_ $frame
		$self build_widget
	}
}


HTTP public enable_output { { yes 1 } } {
	$self set enable_output_ $yes
}


HTTP proc.invoke { } {
	if { [lsearch -exact [package names] Tk] != -1 } {
		font create http_font -family helvetica -size 10
	}
}


Class HTTPCache


HTTPCache public init { {dir ~/.mash/cache/} } {
	$self next

	$self instvar dir_ index_ index_filename_
	$self create_dir $dir
	set dir_ [glob $dir]

	set index_filename_ [file join $dir_ index.db]
	if {! [catch {set f [open $index_filename_]}] } {
		while 1 {
			set line [gets $f]
			if [eof $f] {
				close $f
				break
			}
			set index_([lindex $line 0]) [lindex $line 1]
		}
	}
}


HTTPCache public get { url {last_modified {}} } {
	$self instvar index_
	if [info exists index_($url)] {
		if { $last_modified != {} } {
			if [catch {set mtime [file mtime $index_($url)]}] \
					{ return "" }
			if { $last_modified==-1 || $mtime < $last_modified } \
					{ return "" }
		}
		if [catch {set f [open $index_($url)]}] { return "" }
		fconfigure $f -translation binary
		set buffer ""
		while { ![eof $f] } {
			append buffer [read $f 4096]
		}
		close $f
		return $buffer
	} else {
		return ""
	}
}


HTTPCache public put { url buffer } {
	$self instvar index_ dir_ index_filename_
	if { ![info exists index_($url)] } {
		set update_index_file 1
	}

	set name cache[clock clicks]
	set index_($url) [file join $dir_ $name[file extension $url]]

	set f [open $index_($url) w 0644]

	fconfigure $f -translation binary
	puts -nonewline $f $buffer
	close $f

	# write the index file
	if [catch {set f [open $index_filename_ a]}] {
		set f [open $index_filename_ w 0644]
	}

	puts $f [list $url $index_($url)]
	close $f
}


HTTPCache public flush { } {
	$self instvar index_ dir_
	file delete -force -- [glob -nocomplain [file join $dir_ *]]
	catch {unset index_}
}


HTTPCache private create_dir { path } {
	if { ![file isdirectory $path] } {
		set dir ""
		foreach split [file split $path] {
			set dir [file join $dir $split]
			if { ![file exists $dir] } {
				# this command will cause an error
				# if it is not possible to create the dir
				file mkdir $dir
			}
		}
	}
}


# create an HTTP object
HTTP Http


