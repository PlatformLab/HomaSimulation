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
# $Header: /cvsroot/nsnam/nam-1/tcl/anim-ctrl.tcl,v 1.37 2002/12/17 01:06:28 buchheim Exp $

Class AnimControl 

# Build main menu
AnimControl instproc init {trace_file args} {
	AnimControl instvar PORT_FILE_

	catch "array set opts $args"
	if [info exists opts(s)] {
		AnimControl instvar isSync_
		set isSync_ 1
	}
	if [info exists opts(a)] {
		set flag 1
	} elseif {[file exists $PORT_FILE_] && [file readable $PORT_FILE_]} {
		set flag 0
	} else {
		set flag 1
	}
	if [info exists opts(k)] {
		AnimControl instvar INIT_PORT_
		set INIT_PORT_ $opts(k)
	}
	if $flag {
		# If forced, or we are the first instance, start a 
		# new instance of nam
		$self local-create-animator $trace_file [join $args]
	} else {
		# Try to connect to a "main" instance of nam, then terminate
		$self remote-create-animator $trace_file [join $args]
	}
}

# Stub proc to trigger a AnimControl method
proc ::AnimCtrlOnRemoteRequest args {
	eval [AnimControl instance] on-remote-request $args
}

AnimControl instproc on-remote-request { newsock addr port } {
	# XXX We don't have any admission control!!!

	# Read one line and process the request.
	# XXX Require all request commands are within a single line, and 
	# every word in the command is separated by a *single* white space
	gets $newsock line
	set buf [split $line]
	switch [lindex $buf 0] {
		CA {
			# Create a new animator
			set trace_file [lindex $buf 1]
			# protect any backslashes in the filename
			regsub -all {\\} $trace_file {\\\\} trace_file
			set args [lrange $buf 2 end]
			eval $self create-animator $trace_file $args
		}
		default {
			puts "Unsupported command [lindex $buf 0]"
		}
	}
	close $newsock
}

# Find an appropriate port
AnimControl instproc start-server {} {
	$self instvar NAM_PORT_ NAM_SOCK_
	AnimControl instvar INIT_PORT_ PORT_FILE_
	set NAM_PORT_ $INIT_PORT_

	set ret 1
	while {$ret} {
		incr NAM_PORT_
		set ret [catch {set NAM_SOCK_ \
			[socket -server ::AnimCtrlOnRemoteRequest $NAM_PORT_]}]
		if {$ret} {
			# Failed, delete the socket if it's opened
			# It seems that on FreeBSD if socket fails no file descriptor is left open
			# but this is not the case on solaris. :( 
			if [info exists NAM_SOCK_] {
				close $NAM_SOCK_
			}
			if {$NAM_PORT_ - $INIT_PORT_ > 254} {
				error "Nam failed to create a socket ranging \
from $INIT_PORT_ to $NAM_PORT_."
			}
		}
	}

	# Write that to a lock file, which resides under home directory
	# and is read-only by the user
	set f [open $PORT_FILE_ w 0600]
	puts -nonewline $f $NAM_PORT_
	close $f
}

AnimControl instproc remote-create-animator {trace_file args} {
	catch "array set opts $args"
	AnimControl instvar PORT_FILE_
	if [catch {set f [open $PORT_FILE_ RDONLY]}] {
		error "Cannot read server port from $PORT_FILE_"
	}
	set port [read $f]
	close $f

	if [catch {set sock [socket localhost $port]}] {
		puts -nonewline "Cannot connect to existing nam instance. "
		puts "Starting a new one..."
		$self local-create-animator $trace_file [join $args]
	} else {
		if {$trace_file == ""} {
puts "A nam instance already exists. Use nam <trace file> to view an animation"
			close $sock
		} else {
		global tcl_platform
		if {$tcl_platform(platform) == "windows"} {
			if [regexp {^(\\\\|[A-Za-z]:[/\\])} $trace_file] {
				set tf $trace_file	;# Absolute pathname
			} else {
				set tf [pwd]/$trace_file	;# Relative pathname
			}
		} else {
			if [regexp {^[~/]} $trace_file] {
				set tf $trace_file	;# Absolute pathname
			} else {
				set tf [pwd]/$trace_file ;# Relative pathname
			}
		}
		puts $sock "CA $tf [join $args]"
		flush $sock
		close $sock
		}
	}
}

AnimControl instproc local-init {} {
	$self instvar nam_name wix tagnum animators_

	# TK-related initialization
	. configure -background [option get . background Nam]
	set_app_options [tk appname]

	set nam_name "NAM - The Network Animator v[version]"
	set wix 0
	set tagnum 0
	set animators_ {}

	$self build-menu 
	$self new_webhome 
	$self bind-window .

	AnimControl instvar instance_ 
	if {$instance_ == ""} { 
		set instance_ $self 
	} else {
		error "Couldn't instantiate more than one AnimControl!"
	}

	# Set window title
	#wm title . "[tk appname] console"
	wm title . "Nam Console v[version]"

	global nam_local_display
	set nam_local_display 1
}

AnimControl proc instance {} {
	set ac [AnimControl set instance_]
	if {$ac != ""} {
		return $ac
	}
	error "Couldn't find instance of AnimControl"
}

AnimControl instproc bind-window { w } {
	bind $w <Control-o> "$self on-open"
	bind $w <Control-O> "$self on-open"
	bind $w <Control-q> "$self done"
	bind $w <Control-Q> "$self done"
	bind $w <Control-n> "$self on-new"
	bind $w <Control-N> "$self on-new"
	bind $w <Control-w> "$self on-winlist"
	bind $w <Control-W> "$self on-winlist"
	bind $w <q> "$self done"
	wm protocol $w WM_DELETE_WINDOW "$self done"
}

#----------------------------------------------------------------------
#  Create nam console menubar
#----------------------------------------------------------------------
AnimControl instproc build-menu {} {
	$self instvar nam_name

	frame .menu -relief groove -bd 2
	pack .menu -side top -fill x
	set padx 4

	set mb .menu.file
	set m $mb.m
	menubutton $mb -text "File" -menu $m -underline 0 \
		-borderwidth 1 
		menu $m
        # Launch nam editor
	$m add command -label "New Nam Editor... Ctrl+N" -command "$self on-new"
	$m add command -label "Open... Ctrl+O" -command "$self on-open"
	$m add command -label "WinList Ctrl+W" -command "$self on-winlist"
	$m add separator
	$m add command -label "Quit    Ctrl+Q" -command "$self done"
	pack $mb -side left -padx $padx

	label .menu.name -text $nam_name -font [smallfont] \
		-width 30 -borderwidth 2 -relief groove 
	pack .menu.name -side left -fill x -expand true -padx 4 -pady 1

	set mb .menu.help
	set m $mb.m
	menubutton $mb -text "Help" -menu $m -underline 0 \
		-borderwidth 1 
	menu $m
	$m add command -label "Help" -command "$self new_web help"
	$m add command -label "About nam" -command "$self new_web about"

  	
	pack $mb -side right -padx $padx
}

AnimControl instproc done {} {
	AnimControl instvar PORT_FILE_
	$self instvar animators_ NAM_SOCK_ NAM_PORT_

	if {[llength $animators_] > 0 && \
	    [tk_messageBox -message "Really quit?" -type yesno -icon question \
			-title "nam" -default "no"] == "no"} {
		return
	}

	close $NAM_SOCK_
	if [catch "set f [open $PORT_FILE_ r]"] {
		puts "Nam port file already deleted."
	} else {
		set buf [gets $f]
		close $f
		if {$buf == $NAM_PORT_} {
			file delete $PORT_FILE_
		} else {
			puts "Another nam instance is running..."
		}
	}
	foreach a $animators_ {
		$a done
	}
	destroy .
}

AnimControl instproc on-open {} {
	$self tkvar openFile_ openPeer_


	set openFile_ [tk_getOpenFile \
	    -filetypes {{{NAM} {.nam .nam.gz}} {{Nam Editor} {.ns .enam}}} \
	    -parent .]

	if {$openFile_ == ""} {return}

	set w .openFile
	set openPeer_ ""

	$self do-open-tracefile $w
		
	return

	#XXX all cold below is useless
	set openPeer_ ""

	toplevel .openFile
	set w .openFile

	wm title $w "Open Trace File"
	frame $w.f -borderwidth 1 -relief raised
	pack $w.f -side top -fill both -expand true
	filesel $w.f.sel $w -variable [$self tkvarname openFile_] \
		-command "$self do-open-tracefile $w.f.sel" \
		-label "Trace file to open:"
	pack $w.f.sel -side top -fill both -expand true

	# Peer dialogs
	label $w.pl0 -text "Peer:"
	pack $w.pl0 -side top -anchor w
	entry $w.peerentry -width 50 -relief sunken \
		-textvariable [$self tkvar openPeer_]
	pack $w.peerentry -side top -fill both -expand true

	frame $w.f.btns -borderwidth 0
	pack $w.f.btns -side top -fill x
	button $w.f.btns.save -text Open \
		-command "$self do-open-tracefile $w"
	pack $w.f.btns.save -side left -fill x -expand true
	button $w.f.btns.cancel -text Cancel -command "destroy $w"
	pack $w.f.btns.cancel -side left -fill x -expand true
	set openFile_ [pwd]
}

AnimControl instproc do-open-tracefile {w} {
	$self tkvar openFile_ openPeer_
	checkfile $openFile_ "r" $w.f.sel $w \
		"$self create-animator $openFile_ p=$openPeer_"
}

AnimControl instproc local-create-animator { trace_file args } {
	$self local-init
	$self start-server
	if {$trace_file != ""} {
		$self create-animator $trace_file [join $args]
	}
}

AnimControl instproc create-animator { tracefile args } {
	$self instvar animators_
	AnimControl instvar ANIMATOR_CLASS_

	set fnsets [split $tracefile "."]
        set fnlength [llength $fnsets]
        set ftype [lindex $fnsets [expr $fnlength-1]]
        set ftype [string tolower $ftype]

	if {[string compare $ftype "ns"] == 0  || 
	    [string compare $ftype "enam"] == 0 } {
	# editable nam file

	    set ANIMATOR_CLASS_ Editor
	    set editor [new $ANIMATOR_CLASS_ $tracefile]
	    return
	}

	set ANIMATOR_CLASS_ Animator
	AnimControl instvar isSync_
	if {[info exist isSync_]} {
	    lappend args {s 1}
	}
	if ![catch {eval set anim [new $ANIMATOR_CLASS_ $tracefile \
			[join $args]]} errMsg] {
		lappend animators_ $anim
		$self instvar winlist_
		if [info exists winlist_] {
			$winlist_ insert end [$anim get-name]
		}
	} else {
		global errorInfo
		puts "Failed to start animator: "
		puts "$errMsg"
		puts "$errorInfo"
	}
}

#----------------------------------------------------------------------
# Launch nam editor
#----------------------------------------------------------------------
AnimControl instproc on-new {} {
	$self instvar animators_
	AnimControl instvar ANIMATOR_CLASS_
	set ANIMATOR_CLASS_ Editor
	set editor [new $ANIMATOR_CLASS_ ""]
}


AnimControl instproc close-animator { animator } {
	$self instvar animators_
	set pos [lsearch $animators_ $animator]
	set animators_ [lreplace $animators_ $pos $pos]
	$self instvar winlist_
	if [info exists winlist_] {
		$winlist_ delete $pos
	}
}

# Create a listbox which lists all active windows
AnimControl instproc on-winlist {} {
	$self instvar winlist_ 
	if [info exists winlist_] {
		return
	}

	set w .winList
	toplevel $w

	wm title $w "Active Animations"

        frame $w.f -borderwidth 0 -highlightthickness 0
	frame $w.f.f
	listbox $w.f.f.a -xscrollcommand "$w.f.f.ah set" \
		-yscrollcommand "$w.f.f2.av set" -height 10 \
		-selectmode single 
	pack $w.f.f.a -fill both -side top -expand true 
	set winlist_ $w.f.f.a

	scrollbar $w.f.f.ah -orient horizontal -width 10 -borderwidth 1 \
		-command "$w.f.f.a xview"
	$w.f.f.ah set 0.0 1.0
	pack $w.f.f.ah -side bottom -fill x -expand true

	frame $w.f.f2
	pack $w.f.f2 -side left -fill y
	scrollbar $w.f.f2.av -orient vertical -width 10 -borderwidth 1 \
			-command "$w.f.f.a yview"
	$w.f.f2.av set 0.0 1.0
	pack $w.f.f2.av -side top -fill y -expand true
        pack $w.f.f -side left -fill both -expand true
	pack $w.f -side left -fill both -expand true

	bind $w.f.f.a <Double-ButtonPress-1> "$self winlist_show $w.f.f.a"
	bind $w.f.f.a <Destroy> "+$self unset winlist_"
	bind $w <Control-q> "[AnimControl instance] done"
	bind $w <Control-Q> "[AnimControl instance] done"
	bind $w <Control-w> "destroy $w"
	bind $w <Control-W> "destroy $w"
	bind $w <Escape> "destroy $w"

	# Fill window list
	$self instvar animators_
	foreach a $animators_ {
		$w.f.f.a insert end [$a get-name]
	}
}

AnimControl instproc winlist_show { wlbox } {
	set idx [$wlbox index active]
	$self instvar animators_
	if [llength $animators_] then {
	    raise [[lindex $animators_ $idx] get-tkwin]
	}
}
