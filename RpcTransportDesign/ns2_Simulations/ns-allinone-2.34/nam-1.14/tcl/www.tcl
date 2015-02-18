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
# $Header: /cvsroot/nsnam/nam-1/tcl/www.tcl,v 1.15 2007/02/18 22:44:54 tom_henderson Exp $

# WWW "browser" - setup big table of what affects what
set stackthese "h1 h2 h3 h4 h5 h6 b i ul ol dl a pre"
set www_cancels(/h1) "h1 h2 h3 h4 h5 h6 i b"
set www_cancels(/h2) "h1 h2 h3 h4 h5 h6 i b"
set www_cancels(/h3) "h1 h2 h3 h4 h5 h6 i b"
set www_cancels(/h4) "h1 h2 h3 h4 h5 h6 i b"
set www_cancels(/h5) "h1 h2 h3 h4 h5 h6 i b"
set www_cancels(/h6) "h1 h2 h3 h4 h5 h6 i b"
set www_cancels(h1) "h1 h2 h3 h4 h5 h6 i b"
set www_cancels(h2) "h1 h2 h3 h4 h5 h6 i b"
set www_cancels(h3) "h1 h2 h3 h4 h5 h6 i b"
set www_cancels(h4) "h1 h2 h3 h4 h5 h6 i b"
set www_cancels(h5) "h1 h2 h3 h4 h5 h6 i b"
set www_cancels(h6) "h1 h2 h3 h4 h5 h6 i b"
set www_cancels(/b) "b"
set www_cancels(/strong) "strong"
set www_cancels(/i) "i"
set www_cancels(/ul) "ul"
set www_cancels(/ol) "ol"
set www_cancels(/dl) "dl"
set www_cancels(/a) "a"
set www_cancels(/pre) "pre"
set www_sets(h1) {{bold 1} {size 18}}
set www_sets(h2) {{bold 1} {size 14}} ;# may not exists on some systems
set www_sets(h3) {{bold 1} {size 14}}
set www_sets(h4) {{bold 1} {size 12}}
set www_sets(h5) {{bold 1} {size 10}}
set www_sets(h6) {{bold 1} {size 10}}
set www_sets(i) {{italic 1} {bold 0}}
set www_sets(b) {{italic 0} {bold 1}}
set www_sets(strong) {{italic 0} {bold 1}}
set www_sets(verbatim) {{verbatim 1} {font courier}}
set www_sets(normal) {{bold 0} {italic 0} {font helvetica} {size 12} {nl 0} {hr 0} {ltype ""} {bullet 0} {verbatim 0} {indent 0} {href ""}}
set www_sets(ul) {{ltype ul} {indent [expr $indent+4]}}
set www_sets(ol) {{ltype ol} {indent [expr $indent+4]}}
set www_sets(dl) {{ltype dl} {indent [expr $indent+4]}}
set www_sets(a) {{href XXX}}
set www_runs(a) {extract_anchor}
set www_runs(img) {extract_image}

#www_sets is for tags that get stacked and need to be set continuously
#www_fsets is only run the first time 
set www_fsets(p) {{nl 2}}
set www_fsets(br) {{nl 1}}
set www_fsets(hr) {{hr 1}}
set www_fsets(h1) {{nl 2}}
set www_fsets(h2) {{nl 2}}
set www_fsets(h3) {{nl 2}}
set www_fsets(h4) {{nl 2}}
set www_fsets(h5) {{nl 2}}
set www_fsets(h6) {{nl 2}}
set www_fsets(ul) {{nl 0}}
set www_fsets(ol) {{nl 0}}
set www_fsets(dl) {{nl 0}}
set www_fsets(/h1) {{nl 2}}
set www_fsets(/h2) {{nl 2}}
set www_fsets(/h3) {{nl 2}}
set www_fsets(/h4) {{nl 2}}
set www_fsets(/h5) {{nl 2}}
set www_fsets(/h6) {{nl 2}}
set www_fsets(/ul) {{nl 1}}
set www_fsets(/ol) {{nl 1}}
set www_fsets(/dl) {{nl 1}}
set www_fsets(li) {{nl 1} {bullet 1}}
set www_fsets(dt) {{nl 1}}

set fonts(normal) -*-helvetica-medium-r-normal--12-*
set fonts(helvetica,0,0,16) -*-helvetica-medium-r-normal--17-*
set fonts(helvetica,1,0,16) -*-helvetica-bold-r-normal--17-*
set fonts(helvetica,1,1,16) -*-helvetica-bold-o-normal--17-*
set fonts(helvetica,1,0,18) -*-helvetica-bold-r-normal--18-*
set fonts(helvetica,1,1,18) -*-helvetica-bold-o-normal--18-*
set fonts(helvetica,1,0,14) -*-helvetica-bold-r-normal--14-*
set fonts(helvetica,1,1,14) -*-helvetica-bold-o-normal--14-*
set fonts(helvetica,1,0,12) -*-helvetica-bold-r-normal--12-*
set fonts(helvetica,1,1,12) -*-helvetica-bold-o-normal--12-*
set fonts(helvetica,0,1,12) -*-helvetica-medium-o-normal--12-*
set fonts(helvetica,1,0,10) -*-helvetica-bold-r-normal--10-*

AnimControl instproc parse_html {w entry text} {
    global www_sets www_fsets www_cancels www_runs stackthese fonts 
    $self instvar href href_keep www_win
    set www_win $w
    set stack normal
    set size normal
    set font helvetica
    set bold 0
    set italic 0
    set underline 0
    set verbatim 0
    set indent 0
    set nl 0
    set hr 0
    set prevnl 0
    set href ""
    set bullet 0
    set ltype ""
    set parts [split $text "<>"]
    set istag 0
    $w delete 1.0 end
    foreach part $parts {
	if {$istag==1} {
	    set tag [string tolower [lindex $part 0]]
	    set c ""
	    catch {set c $www_cancels($tag)}
	    set rm 0
	    set newstack {}
	    foreach frame $stack {
		set newframe {}
		if {([lsearch -exact $c $frame]>=0)&&($rm==0)} {
		    set rm 1
		} else {
		    lappend newstack $frame
		}
	    }
	    set stack $newstack
	    set newframe $tag
#	    puts "$tag, $stack"

	    #set all the stuff corresponding to this stack
	    foreach frame "$stack $newframe" {
		set t [lindex $frame 0]
		set s ""
		catch {set s $www_sets($t)}
		foreach item $s {
		    set cmd "set $item"
#		    puts "    $cmd"
		    eval $cmd
		}
	    }

	    #now deal with the one off stuff for this tag
	    set s ""
	    catch {set s $www_fsets($tag)}
	    foreach item $s {
		set cmd "set $item"
#		puts "    $cmd"
		eval $cmd
	    }

	    #now run any specific code for this tag
	    set s ""
	    catch {set s $www_runs($tag)}
	    foreach item $s {
		set cmd "$self $item $part"
#		puts "    $cmd"
		eval $cmd
	    }

	    if {[lsearch -exact $stackthese $tag]>=0} {
		lappend stack $newframe
	    }

	} else {
	    if {$hr==1} {
		$w insert end "\n_____________________________________________________\n"
		if {$prevnl==0} {set prevnl 1}
	    }

	    #handle all the newlines
	    for {set i 0} {$i < [expr $nl-$prevnl]} {incr i} {
		$w insert end "\n"
	    }
	    set prevnl $nl

	    #sort out the indentation
	    if {$nl>0} {
		for {set i 0} {$i < $indent} {incr i} {
		    $w insert end " "
		}
	    }

	    #add any bullets
	    if {$bullet!=0} {
		$w insert end "* "
	    }

	    set ix [$w index end-1c]
	    if {$nl>0} {
		set part [string trimleft $part]
	    }
	    if {$verbatim==0} {
		set part [remove_nl $part]
		if {[string length $part]>0} {
		    set prevnl 0
		}
		$w insert end $part
	    } else {
		$w insert end $part
	    }

	    $self instvar tagnum
	    if {($size!=12)||($bold==1)||($italic==1)||($href!="")||\
		    ($font!="helvetica")} {
		incr tagnum
		set ix2 [$w index end-1c]
		$w tag add t$tagnum $ix end-1c
	    }
	    if {($size!=12)||($bold==1)||($italic==1)||\
		    ($font!="helvetica")} {
		set thisfont $fonts(normal)
		catch {
		    set thisfont $fonts($font,$bold,$italic,$size)
		}
		$w tag configure t$tagnum -font $thisfont
	    }
	    if {$href!=""} {
		$w tag configure t$tagnum -foreground red
		$w tag bind t$tagnum <Enter> \
			"$self set_entry $entry $href_keep"
		$w tag bind t$tagnum <Leave> \
			"$self set_entry $entry {}"
		$w tag bind t$tagnum <1> "$self goto $href_keep $w $entry"
	    }
	    
	}
	set istag [expr 1-$istag]
    }
}

AnimControl instproc set_entry {entry str} {
    $entry delete 0 end
    $entry insert 0 $str
}

AnimControl instproc extract_anchor args {
    $self instvar href_keep href
    set href_keep ""
    set href ""
    foreach arg $args {
	if {[string range [string tolower $arg] 0 4]=="href="} {
	    set href_keep [string trim [string range $arg 5 end] {"}]
	    set href XXX
	}	    
    }
}

AnimControl instproc extract_image args {
    $self instvar www_win wix
    set img ""
    foreach arg $args {
	if {[string range [string tolower $arg] 0 3]=="src="} {
	    set img [string trim [string range $arg 4 end] {"}]
	}	    
    }
    $www_win window create end -create "label $www_win.w$wix -bitmap $img"
    incr wix
}

AnimControl instproc goto {url win entry} {
    global help_text
    set res "xxxx"
    set scheme [lindex [split $url ":"] 0]
    if {$scheme=="help"} {
	$self parse_html $win $entry $help_text([string range $url 5 end])
	return
    }
    set proggy netscape
    catch {
        set res [exec $proggy -display :0 -remote openURL($url)]
    }
    if {$res=="xxxx"} {
        catch {
            set res [exec $proggy -display :0.1 -remote openURL($url)]
        }
    }
    if {$res=="xxxx"} {
        puts "failed to pass URL to $proggy"
    }
}

proc lreverse {list} {
    set res {}
    foreach el $list {
	set res [concat [list $el] $res]
    }
    return $res
}

proc remove_nl {str} {
    set parts [split $str "\n\r"] 
    set res ""
    set space 1
    for {set i 0} {$i<[llength $parts]} {incr i} {
	set part [lindex $parts $i]
	if {$i>0} {
	    set part [string trimleft $part]
	} else {
	    if {$part==""} {set space 0}
	}
	if {($i+1)<[llength $parts]} {
	    set part [string trimright $part]
	}
	if {$space==0} {
	    set res "$res "
	    set space 1
	} 
	if {[string length $part] > 0} {
	    set res "$res$part"
	    set space 0
	}
    }
    return $res
}

set help_text(home) "<h1>NAM - The Network Animator</h1>
Welcome to Nam 1.14
<P>
Developed by UCB and the VINT, SAMAN, and Conser projects at ISI.
<P>
Nam contains source code with the following copyrights:
<P>
Copyright (c) 1991-1994 Regents of the University of California.
<BR>
Copyright (c) 1997-1999 University of Southern California
<BR>
Copyright (c) 2000-2002 USC/Information Sciences Institute
<P>
"


set help_text(about) "<h1>NAM - The Network Animator</h1>
This version of nam is highly experimental - there will be bugs!.  
Please mail <I><a href=mailto:ns-users@isi.edu>ns-users@isi.edu</a></I> if you 
encounter any bugs, or with suggestions for desired functionality.
<H2>History</h2>
The network animator ``nam''
began in 1990 as a simple tool for animating packet trace data.
This trace data is typically
derived as output from a network simulator like
<a href=http://www-mash.cs.berkeley.edu/ns/>ns</a>
or from real network measurements, e.g., using
<a href=ftp://ftp.ee.lbl.gov/tcpdump.tar.Z>tcpdump</a>.
<a href=http://www.cs.berkeley.edu/~mccanne>Steven McCanne</a>
wrote the original version as a member of the
<a href=http://www-nrg.ee.lbl.gov/>Network Research Group</a> at the
<a href=http://www.lbl.gov/>Lawrence Berkeley National Laboratory</a>,
and occasionally improved the design as he needed it in
his research.  Marylou Orayani improved it further and used it
for her Master's research over summer 1995 and into spring 1996.
The nam development effort is now an ongoing
collaboration at ISI in the SAMAN and Conser projects.
Nam developers include:
<a href=http://www.isi.edu/nsnam/>SAMAN and Conser projects</a>.
<a href=http://netweb.usc.edu/vint/>VINT project</a>.
<a href=http://north.east.isi.edu/~mjh/>Mark Handley</a>, 
<a href=http://www.isi.edu/~haoboy/>Haobo Yu</a>, <a href=http://www.isi.edu/~johnh/>John Heidemann</a>,
 <a href=http://www.isi.edu/~yaxu/>Ya Xu</a>, <a href=http://www.isi.edu/~kclan/>Kun-chan Lan</a>, and <a href=http://www-scf.usc.edu/~hyunahpa/>Hyunah Park</a>.
<P>
For information about ns and nam, please see
<i><a href=http://mash.cs.berkeley.edu/nam/>http://mash.cs.berkeley.edu/nam/</a></i>.
<h2>Funding</h2>
Nam is currently funded by DARPA through the VINT project at LBL under
DARPA grant DABT63-96-C-0105, at USC/ISI under DARPA grant
ABT63-96-C-0054, at Xerox PARC under DARPA grant DABT63-96-C-0105. 
<H2>Copyright</H2>
Nam contains source code with the following copyrights:
<p>
Copyright (c) 1991-1994 Regents of the University of California.
<br>
Copyright (c) 1997-1999 University of Southern California
<P>
Copyright (c) 2000-2001 USC/Information Sciences Institute
<hr>
<i><a href=help:help>Help Index</a><I>
"

set help_text(help) "
<H1>Nam Quick Help</H1>
<B>Use the controls to move the animation through time:</B>
<DL>
<DT><img src=rew>Rewind the animation by 25 times the current time step
<DT><img src=back>Play the animation backwards
<DT><img src=stop>Stop the playing of the animation
<DT><img src=play>Play the animation normally
<DT><img src=ff>Fast forward the animation by 25 times the current time step
</DL>
The current time step is selected by the time slider.
<P>
When the animation is running
<UL><LI>Click the left button on an object in the display window to view information about that object.
<LI>Select \"monitor\" from the resulting popup window to <a href=help:monitors>monitor that object</a> over time.
<LI>The \"step\" slider can be used to control how fast time flows in the animation.
The \"time\" slider can be used to move to a specific point in time.
</UL>
<P>
<hr>
<i><a href=help:about>About Nam</a><I>
"

set help_text(monitors) "
<H1>Nam Help: Monitors</H1>
You can set monitors on various animation objects to examine 
information about them over time.
<H2>Packet Monitors</H2>
To initiate a packet monitor, click the left button on the packet 
you wish to examine.  Information that nam has about that packet 
will then be displayed, and you are given the option to set a 
monitor on the packet.
<P>
Setting a monitor on a packet labels the packet so that you can 
more easily watch its flow through the network and through queues.
An associated panel appears in the monitors window listing information 
about the packet if the packet is visible.  
<P>
Click on the monitor panel for a packet to remove that monitor.
<H2>Agent Monitors</H2>
Protocol agents are displayed alongside the node they are 
instantiated in.  To monitor an agent, click on it in the 
network display, and select \"monitor\".
Information about the agent will be displayed in the monitors 
panel, and will be updated as protocol state in the agent 
changes over time.
<P>
Click on the monitor panel for the agent to remove that monitor.
<hr>
<I><a href=help:help>Return to help index</a>
"

AnimControl instproc new_web {which} {
    global help_text
    if {[winfo exists .help]} {
	$self parse_html .help.text .help.f.ctl.e $help_text($which)
	return
    }
    toplevel .help
    wm title  .help "About nam"
    frame .help.f -relief groove -borderwidth 2
    pack  .help.f -side top -fill both -expand true
    frame .help.f.f
    pack  .help.f.f -side top -fill both -expand true
    text .help.f.f.w -width 60 -height 40 -wrap word -yscroll ".help.f.f.sb set"
    bind .help.f.f.w <1> break
    pack .help.f.f.w -side left -fill both -expand true
    scrollbar .help.f.f.sb -command ".help.f.f.w yview"
    pack .help.f.f.sb -side right -fill y
    frame .help.f.ctl -borderwidth 2 -relief groove
    pack .help.f.ctl -side top -fill x
    label .help.f.ctl.l -text "go to:"
    pack .help.f.ctl.l -side left

    entry .help.f.ctl.e -width 20 -borderwidth 1 -relief sunken
    pack .help.f.ctl.e -side left -fill x -expand true
    button .help.f.ctl.b -borderwidth 1 -relief raised -text Dismiss \
	    -command {destroy .help}
    pack .help.f.ctl.b -side left
    $self parse_html .help.f.f.w .help.f.ctl.e $help_text($which)
}

#-----------------------------------------------------------------------
# Creates nam console splash screen
#-----------------------------------------------------------------------
AnimControl instproc new_webhome {} {
    global help_text
    if {[winfo exists .help]} {
        $self parse_html .help.f.f.w .help.f.ctl.e $help_text(hich)
        return      
    }           

    set t [$self ScrolledText .help 60 15]

    frame .help.f -relief groove -borderwidth 2
    frame .help.f.ctl -borderwidth 2 -relief groove
    entry .help.f.ctl.e -width 20 -borderwidth 1 -relief sunken
    pack .help.f.ctl.e -side left -fill x -expand true
    button .help.f.ctl.b -borderwidth 1 -relief raised -text Dismiss \
            -command {destroy .help}
    pack .help.f.ctl.b -side left
    $self parse_html $t .help.f.ctl.e $help_text(home)
}

AnimControl instproc ScrolledText { f width height } {
        frame $f
        # The setgrid setting allows the window to be resized.
        text $f.text -width $width -height $height \
                -setgrid true -wrap word \
                -xscrollcommand [list $f.xscroll set] \
                -yscrollcommand [list $f.yscroll set]
        scrollbar $f.xscroll -orient horizontal \
                -command [list $f.text xview]
        scrollbar $f.yscroll -orient vertical \
                -command [list $f.text yview]
        pack $f.xscroll -side bottom -fill x
        pack $f.yscroll -side right -fill y
      
        # The fill and expand are needed when resizing.
        pack $f.text -side left -fill both -expand true
        pack $f -side top -fill both -expand true
        return $f.text
}

