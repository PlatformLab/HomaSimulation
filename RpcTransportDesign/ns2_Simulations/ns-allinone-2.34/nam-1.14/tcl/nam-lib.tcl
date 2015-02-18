#
# Copyright (c) 1993-1998 Regents of the University of California.
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
#	This product includes software developed by the Computer Systems
#	Engineering Group at Lawrence Berkeley Laboratory.
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
# @(#) $Header: /cvsroot/nsnam/nam-1/tcl/nam-lib.tcl,v 1.23 2001/08/17 02:12:40 mehringe Exp $ (LBL)
#

proc time2real v {
	global uscale

	foreach u [array names uscale] {
		set k [string first $u $v]
		if { $k >= 0 } {
			set scale $uscale($u)
			break
		}
	}
	if { $k > 0 } {
		set v [string range $v 0 [expr $k - 1]]
		set v [expr $scale * $v]
	}
	return $v
}
#XXX
proc bw2real v {
	return [time2real $v]
}

proc step_format t {
	if { $t < 1e-3 } {
		return [format "%.1f" [expr $t * 1e6]]us
	} elseif { $t < 1. } {
		return [format "%.1f" [expr $t * 1e3]]ms
	}
	return [format "%.1f" $t]s
}

proc yesno attr {
	set v [resource $attr]
	if { [string match \[0-9\]* $v] } {
		return $v
	}
	if { $v == "true" || $v == "True" || $v == "t" || $v == "y" ||
		$v == "yes" } {
		return 1
	}
	return 0
}

proc resource s {
	return [option get . $s Nam]
}

#XXX
proc mapf s { return $s }

# XXX
# I got a problem with the font setups in nam.tcl. Currently they are added
# into the Tk option database by "option add Nam.<name> <font name>
# <priority>" It seems to me that this requires the application name (as    
# initialized in Tk_Init() or set by Tk_SetAppName) to be "nam".
#
# This is not desirable, however, because sometimes when we need to compare
# two trace files, we'd like to use the peer functionality to synchronize   
# the execution of two nam instances. It requires that different nam
# instances have different application names. When I allow 
# customized application names, I found that nam no longer recognizes the   
# font resources, e.g., Nam.smallfont. Then I put all the following setups 
# starting with "Nam." into the following function. 
# 
# Q: Is this harmful? Any better ideas?
proc set_app_options { name } {
	global helv10 helv10b helv10o helv12 helv12b helv14 helv14b times14 \
		ff tcl_platform

	option add $name.foundry adobe startupFile
	set ff [option get . foundry $name]
	
	set helv10 [mapf "-$ff-helvetica-medium-r-normal--*-100-75-75-*-*-*-*"]
	set helv10b [mapf "-$ff-helvetica-bold-r-normal--*-100-75-75-*-*-*-*"]
	set helv10o [mapf "-$ff-helvetica-bold-o-normal--*-100-75-75-*-*-*-*"]
	set helv12 [mapf "-$ff-helvetica-medium-r-normal--*-120-75-75-*-*-*-*"]
	set helv12b [mapf "-$ff-helvetica-bold-r-normal--*-120-75-75-*-*-*-*"]
	set helv14 [mapf "-$ff-helvetica-medium-r-normal--*-140-75-75-*-*-*-*"]
	set helv14b [mapf "-$ff-helvetica-bold-r-normal--*-140-75-75-*-*-*-*"]
	set times14 [mapf  "-$ff-times-medium-r-normal--*-140-75-75-*-*-*-*"]

	if {$tcl_platform(platform)!="windows"} {
		option add *font $helv12b startupFile
		option add *Font $helv12b startupFile
	}

        option add *Balloon*background yellow widgetDefault
        option add *Balloon*foreground black widgetDefault
        option add *Balloon.info.wrapLegnth 3i widgetDefault
        option add *Balloon.info.justify left widgetDefault
        option add *Balloon.info.font \
                [mapf "-$ff-lucida-medium-r-normal-sans-*-120-*"] widgetDefault

	option add $name.disablefont $helv10o startupFile
	option add $name.smallfont $helv10b startupFile
	option add $name.medfont $helv12b  startupFile
	option add $name.helpFont $times14 startupFile
	option add $name.entryFont $helv10 startupFile

	option add $name.rate		2ms	startupFile
	option add $name.movie		0	startupFile
	option add $name.granularity	40	startupFile
	option add $name.pause		1	startupFile
}

if {$tcl_platform(platform)!="windows"} {
	option add *foreground black startupFile
	option add *background gray80 startupFile
	option add *activeForeground black startupFile
	option add *activeBackground gray90 startupFile
	option add *viewBackground gray90 startupFile
	option add *disabledForeground gray50 startupFile
    
} else {
	# We need a default foreground for dialog boxes 
	# (checkfile in menu_file.tcl)
	option add *foreground SystemButtonText startupFile
	option add *background SystemButtonFace startupFile
	option add *viewBackground SystemButtonHighlight startupFile
	option add *activeForeground SystemHighlightText startupFile
	option add *activeBackground SystemHighlight startupFile
	option add *disabledForeground SystemDisabledText startupFile
}

option add *Radiobutton.relief flat startupFile

#
# use 2 pixels of padding by default
#
option add *padX 2 startupFile
option add *padY 2 startupFile
#
# don't put tearoffs in pull-down menus
#
option add *tearOff 0 startupFile

proc smallfont { } {
	return [option get . smallfont Nam]
}
proc mediumfont { } {
	return [option get . medfont Nam]
}

#
# helper functions
#
proc nam_angle { v } {
	switch -regexp $v {
	       ^[0-9\.]+deg$    { regexp  {([0-9\.]+)deg$} $v a dval
     			          return [expr {fmod($dval,360)/180}] }

		^[0-9\.]+$	{ return $v }

		^up-right$ -
		^right-up$	{ return 0.25 }


		^up$		{ return 0.5 }

		^up-left$ -
		^left-up$	{ return 0.75 }

		^left$		{ return 1. }

		^left-down$ -
		^down-left$	{ return 1.25 }

		^down$		{ return 1.5 }

		^down-right$ -
		^right-down$	{ return 1.75 }

		default		{ return 0.0 }
	}
}

proc remote_cmd { async interp cmd } {
	if $async {
		set rcmd "send -async \"$interp\" {$cmd}"
	} else {
		set rcmd "send \"$interp\" {$cmd}"
	}
	eval $rcmd
}

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

# Place a frame at (x, y)
proc place_frame { w f x y } {
	# Place popup menu. Copied from start_info()
	set nh [winfo height $w]
	set nw [winfo width $w]
	place $f -x $nw -y $nh -bordermode outside
	update
	set ih [winfo reqheight $f]
	set iw [winfo reqwidth $f]
	set px $x
	set py $y
	if {($px + $iw) > $nw} {
		set px [expr $x - $iw]
		if {$px < 0} { set px 0 }
	}
	if {($py + $ih) > $nh} {
		set py [expr $y - $ih]
		if {$py < 0} {set py 0}
	}
	place $f -x $px -y $py -bordermode outside
}


# Sourcing various TCL scripts
# In order to be expanded, there shouldn't be any spaces, etc., after 
# file names

source anim-ctrl.tcl
source animator.tcl
source balloon.tcl
source snapshot.tcl
source autoNetModel.tcl
source annotation.tcl
source node.tcl
source netModel.tcl
source menu_file.tcl
source menu_view.tcl
source monitor.tcl
source www.tcl
source build-ui.tcl
source stats.tcl
source observer.tcl
source observable.tcl
source wirelessNetModel.tcl
source NamgraphView.tcl
source NamgraphModel.tcl
source TimesliderModel.tcl
source TimesliderView.tcl
source TimesliderNamgraphView.tcl
source Editor.tcl
#source EditorMenu.tcl
source Editor-FileParser.tcl
source editorNetModel.tcl

source nam-default.tcl


proc nam_init { trace_file args } {
        # Create animation control object
        # anim-ctrl.tcl: AnimControl instproc init{}
	set actrl [new AnimControl $trace_file $args]

	# User-defined initialization hook
	nam_init_hook
}

# XXX
# User-defined initialization hook. Perhaps we need an OO approach here?
proc nam_init_hook {} {
}


# Random number generator initialization
set defaultRNG [new RNG]
$defaultRNG seed 1
$defaultRNG default
#
# Because defaultRNG is not a bound variable but is instead
# just copied into C++, we enforce this.
# (A long-term solution be to make defaultRNG bound.)
# --johnh, 30-Dec-97
#
trace variable defaultRNG w { abort "cannot update defaultRNG once assigned"; }

