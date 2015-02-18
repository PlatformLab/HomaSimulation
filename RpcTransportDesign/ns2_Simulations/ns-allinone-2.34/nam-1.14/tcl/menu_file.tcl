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
# $Header: /cvsroot/nsnam/nam-1/tcl/menu_file.tcl,v 1.5 2000/02/16 01:01:11 haoboy Exp $

#
# Helper functions
#
proc filesel {w dlg args} {
    frame $w -borderwidth 0
    set next ""
    set ctr 0
    foreach arg $args {
	if {[string range $arg 0 0]=="-"} {
	    switch [string range $arg 1 end] {
		"variable" {
		    set next variable
		}
		"command" {
		    set next command
		}
		"label" {
		    set next label
		}
	    }
	} else {
	    if {$next!=""} {
		set vars($next) $arg
	    }
	}
    }
    global $vars(variable)
    set $vars(variable) [pwd]
    frame $w.f0 -relief sunken -borderwidth 2
    listbox $w.f0.lb -width 50 -height 10 \
        -yscroll "$w.f0.sb set" \
        -selectforeground [resource activeForeground] \
        -selectbackground [resource activeBackground] \
        -highlightthickness 0
    scrollbar $w.f0.sb -command "$w.f0.lb yview" \
        -highlightthickness 0
    pack $w.f0.lb -side left
    pack $w.f0.sb -side left -fill y
    pack $w.f0 -side top

    # We should allow flexible labeling 
    label $w.l2 -text $vars(label)
    pack $w.l2 -side top -anchor w

    entry $w.entry -width 50 -relief sunken
    bind $w.entry <Return> \
	"checkfile \[set $vars(variable)\] \"w\" $w $dlg \"$vars(command)\""
    $w.entry insert 0 [pwd]
    bind $w.entry <Delete> \
	"[bind Entry <BackSpace>];\
	set $vars(variable) \[$w.entry get\];break"
    bind $w.entry <BackSpace> \
	"[bind Entry <BackSpace>];\
	set $vars(variable) \[$w.entry get\];break"
    bind $w.entry <Key> \
	"[bind Entry <Key>];\
	set $vars(variable) \[$w.entry get\];break"
    bind $w.f0.lb <1> "%W selection set \[%W nearest %y\];\
	$w.entry delete 0 end;\
	set $vars(variable) \[pwd\]/\[%W get \[%W nearest %y\]\]; \
	$w.entry insert 0 \[pwd\]/\[%W get \[%W nearest %y\]\]"
    bind $w.f0.lb <Double-Button-1> \
	"checkfile \[set $vars(variable)\] \"w\" $w $dlg \"$vars(command)\""
    pack $w.entry -side top -anchor w -fill x -expand true
    
    checkfile [pwd] "r" $w $dlg ""
    return $w
}

proc checkfile {file op w dlg cmd} {
    if {[file isdirectory $file]} {
	$w.entry delete 0 end
	cd $file
	$w.entry insert 0 [pwd]
	$w.f0.lb delete 0 end
	# insert the parent dir, because glob won't generate it
	$w.f0.lb insert end ..
	foreach i [glob *] {
		$w.f0.lb insert end $i
	}
    } elseif {[file isfile $file]} {
	    if {$op == "r"} {
		    set flag [file readable $file]
	    } else {
		    set flag [file writable $file]
	    }
	    if $flag {
		    eval $cmd
		    destroy $dlg
	    } else {
		    errorpopup "Error" "File $file is not writable"
	    }
    } else {
	    # Is it a wild card filter?
	    set dirname [file dirname $file]
	    set fname [file tail $file]
	    if {$dirname != ""} {
		    if [catch "cd $dirname"] {
			    # bail out
			    eval $cmd
			    destroy $dlg
		    }
	    }
	    set flist [glob -nocomplain $fname]
	    $w.entry delete 0 end
	    $w.entry insert 0 [pwd]
	    $w.f0.lb delete 0 end
	    if [llength $flist] {
		    $w.f0.lb insert end ..
		    foreach i $flist {
			    $w.f0.lb insert end $i
		    }
	    } else {
		    # not expandable, try write it
		    if {$op == "w"} {
			    eval $cmd
			    destroy $dlg
		    } else {
			    errorpopup "Error" "File $file is not writable"
		    }
	    }
    }
}


Animator instproc save_layout {} {
    $self instvar netModel tlw_
    $self tkvar savefile

    toplevel $tlw_.save
    set w $tlw_.save

    wm title $w "Save layout"
    frame $w.f -borderwidth 1 -relief raised
    pack $w.f -side top -fill both -expand true
    filesel $w.f.sel $w -variable [$self tkvarname savefile] \
	    -command "$netModel savelayout \[set [$self tkvarname savefile]\]" \
	    -label "File to save to:"
    pack $w.f.sel -side top -fill both -expand true
    frame $w.f.btns -borderwidth 0
    pack $w.f.btns -side top -fill x
    button $w.f.btns.save -text Save \
	    -command "$self do_save_layout $w.f.sel $w"
    pack $w.f.btns.save -side left -fill x -expand true
    button $w.f.btns.cancel -text Cancel -command "destroy $w"
    pack $w.f.btns.cancel -side left -fill x -expand true
    set savefile [pwd]
}

Animator instproc do_save_layout {w dlg} {
	$self instvar netModel
	$self tkvar savefile
	set t $savefile
	puts "save to file $t"
	checkfile $t "w" $w $dlg "$netModel savelayout $t"
}

Animator instproc disablefile {w} {
    $w.entry configure -state disabled \
	    -foreground [option get . disabledForeground Nam]
    $w.l2 configure -foreground [option get . disabledForeground Nam]
    $w.f0.lb configure -foreground [option get . disabledForeground Nam]
}

Animator instproc normalfile {w filevar} {
    $self tkvar $filevar
    $w.entry configure -state normal \
	    -foreground [option get . foreground Nam]
    set $filevar [$w.entry get]
    $w.l2 configure -foreground [option get . foreground Nam]
    $w.f0.lb configure -foreground [option get . foreground Nam]
}

Animator instproc print_view { viewobject windowname } {
    $self tkvar printfile printto 
    $self instvar tlw_	

    set p print
 
    if {[winfo exists $tlw_.$windowname-$p]} {
        puts "Quit due to duplicate print view window!"
    }

#    toplevel .print
#    set w .print
#    wm title $w "Print nam animation"

    toplevel $tlw_.$windowname-$p
    set w $tlw_.$windowname-$p
    wm title $w "Print $windowname"    

    frame $w.f -borderwidth 1 -relief raised
    pack $w.f -side top -fill both -expand true
    frame $w.f.f0 
    pack $w.f.f0 -side top -fill x
    label $w.f.f0.l -text "Print to:"
    pack $w.f.f0.l -side left

    $self tkvar printto
    radiobutton $w.f.f0.r1 -text "Printer" -variable [$self tkvarname printto] \
	    -value printer -command "$self disablefile $w.f.sel;\
	    $w.f.f1.e configure -state normal -foreground \
	    [option get . foreground Nam];\
	    $w.f.f1.l configure -foreground [option get . foreground Nam];\
	    set [$self tkvarname printfile] \[$w.f.f1.e get\]"
    pack $w.f.f0.r1 -side left 
    radiobutton $w.f.f0.r2 -text "File" -variable [$self tkvarname printto] \
	    -value file -command "$self normalfile $w.f.sel printfile;\
	    $w.f.f1.e configure -state disabled -foreground \
	    [option get . disabledForeground Nam];\
	    $w.f.f1.l configure -foreground \
	    [option get . disabledForeground Nam]"
    pack $w.f.f0.r2 -side left 
    frame $w.f.f1 
    pack $w.f.f1 -side top -fill x
    label $w.f.f1.l -text "Print command:"
    pack $w.f.f1.l -side left
    entry $w.f.f1.e 
    pack $w.f.f1.e -side left -fill x -expand true
    bind $w.f.f1.e <Key> "[bind Entry <Key>];\
	    set [$self tkvarname printfile] \[$w.f.f1.e get\];break"
    bind $w.f.f1.e <Return> \
	    "$self print_animation $viewobject $w"
    set printer ""
    global env
    catch {set printer $env(PRINTER)}
    if {$printer!=""} {
	$w.f.f1.e insert 0 "lpr -P$printer "
    } else {
	$w.f.f1.e insert 0 "lpr "
    }
    filesel $w.f.sel $w -variable [$self tkvarname printfile] \
	    -command "$viewobject psview \[set [$self tkvarname printfile]\]" \
	    -label "File to save to:"
    pack $w.f.sel -side top -fill both -expand true
    frame $w.f.btns -borderwidth 0
    pack $w.f.btns -side top -fill x
    button $w.f.btns.print -text Print \
	    -command "$self print_animation $viewobject $w"
    pack $w.f.btns.print -side left -fill x -expand true
    button $w.f.btns.cancel -text Cancel -command "destroy $w"
    pack $w.f.btns.cancel -side left -fill x -expand true

    set printto printer
    set printfile [$w.f.f1.e get]
    $self disablefile $w.f.sel
}

Animator instproc print_animation {viewobject w} {
	$self tkvar printto printfile

	if {$printto=="file"} {
		checkfile $printfile "w" $w.f.sel $w \
				"$viewobject psview $printfile"
	} else {
		$viewobject psview /tmp/nam.ps
		puts [eval "exec $printfile /tmp/nam.ps"]
		catch {destroy $w}
	}
}

Animator instproc show_subtrees {} {
    $self instvar netModel
    $netModel showtrees
}
