#Copyright (C) 1998 by USC/ISI
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

Class NamgraphView -superclass Observer

NamgraphView set WinWidth  600
NamgraphView set WinHeight 400

NamgraphView instproc init { title ob minx maxx miny maxy tag } {

    $self next

    $self instvar id_ title_ modelobj_ winname_ minx_ maxx_ miny_ maxy_
    $self instvar slidetag_ timeslider

    set title_ $title
    set modelobj_ $ob
    set slidetag_ $tag

    #window's name
    set winname_ .namgraph-$id_

    set minx_ $minx
    set miny_ $miny
    set maxx_ $maxx
    set maxy_ $maxy

    if { $slidetag_ != 1 } {
       set timeslider(swidth) [$modelobj_ set timeslider_swidth_]
    }

    $self formwindow

}


NamgraphView instproc formwindow {} {

    $self instvar title_ winname_ minx_ maxx_ miny_ maxy_ id_ modelobj_
    $self instvar slidetag_ timeslider

    toplevel $winname_

    set s $winname_
    wm title $s "$title_-$id_"

    set welcome [Animator set welcome]
 

    #
    # Menubar
    #
    set width_ [NamgraphView set WinWidth]
    set height_ [NamgraphView set WinHeight]
    frame $s.menubar -bd 2 -width $width_ -relief raised

    menubutton $s.menubar.file -underline 0 -text "File" \
        -menu $s.menubar.file.menu
    menu $s.menubar.file.menu
    $s.menubar.file.menu add command -label "Save As ..." \
        -underline 0 -command "puts OJ" \
        -state disabled

    # collect garbage ???

    $s.menubar.file.menu add command -label "Close" \
        -underline 1 -command "destroy $winname_" 

    menubutton $s.menubar.edit -underline 0 -text "Edit" \
        -menu $s.menubar.edit.menu -state disabled
    menu $s.menubar.edit.menu
    $s.menubar.edit.menu add command -label "Preferences" \
        -underline 0 -command "puts OJ" -state disabled

    # find menu items in the model

    menubutton $s.menubar.view -underline 0 -text "View" \
        -menu $s.menubar.view.menu
    menu $s.menubar.view.menu

    foreach menuindex_ [$modelobj_ array names dataname] {
        set menuname_ [$modelobj_ set dataname($menuindex_)]
	$self tkvar $menuindex_
  	set $menuindex_ 1
        $s.menubar.view.menu add checkbutton -label $menuname_ \
	    -variable [$self tkvarname $menuindex_] \
	    -command "$self RefreshWin $menuindex_"
    }
	
    # Mix old pack with new grid
    pack $s.menubar.file -side left
    pack $s.menubar.edit -side left
    pack $s.menubar.view -side left


    #
    # keyboard shortcuts
    #

    bind $s <Control-w> "destroy $s"
    bind $s <Control-W> "destroy $s"
    bind $s <Control-q> "[AnimControl instance] done"
    bind $s <Control-Q> "[AnimControl instance] done"


    #   
    # Main Area
    #
    
    frame $s.main -bd 2
    canvas $s.main.c -width $width_ -height $height_ -background #ffffff

    # bind action for zoom etc.

    $s.main.c bind clickable <Button-1> "$self startclick \%W \%x \%y"
    $s.main.c bind clickable <Enter> "$self showclick \%W \%x \%y"
    $s.main.c bind clickable <Leave> "$self endshow \%W \%x \%y"
    $s.main.c bind clickable <ButtonRelease-1> "$self endclick \%W \%x \%y"

    bind $s.main.c <Button-1> "$self startrect \%W \%x \%y"
    bind $s.main.c <B1-Motion> "$self dragrect \%W \%x \%y"
    bind $s.main.c <ButtonRelease-1> "$self endrect \%W \%x \%y"

    canvas $s.main.c.cl -width 16 -height 400 -background #6b7787
    canvas $s.main.c.cr -width 16 -height 400 -background #6b7787
    pack $s.main.c.cl -side left -fill y
    pack $s.main.c.cr -side right -fill y
    pack $s.main.c -side left -fill both -expand yes
    $self build.c0 $s.main.c

    # Synchronize timerslider
#    if { $slidetag_ == 1 } {
# 	set an_id [$modelobj_ set animator_id_]
#        frame $s.slider -bd 1 -relief flat
#        frame $s.slider.timer -bd 1 -relief flat
#        $an_id build-slider $s.slider.timer
#        pack  $s.slider.timer -side left -expand true -fill x
#    }

    if { $slidetag_ == 1 } {
        frame $s.slider -bd 1 -relief flat
        set an_id [$modelobj_ set animator_id_]
        set mslider [$an_id getmainslidermodel]
        set vslider [new TimesliderNamgraphView $s.slider $mslider]
        $vslider setattachedView $self
        set timeslider(swidth) [$vslider set timeslider(swidth)]

        $modelobj_ set timeslider_swidth_ $timeslider(swidth)

        $mslider addObserver $vslider
    }

    # Status Area
    
    frame $s.status -relief flat -borderwidth 1  -background #6b7787
    label $s.status.rem -text $welcome -textvariable \
        remark -relief sunken
    pack $s.status.rem -side left -fill both -expand yes


    # All widgets are in the same column;
    # and each take up one row.
    #
    # Each widget is sticky to all four edges
    #   so that it expands to fit.
    #
    grid config $s.menubar -column 0 -row 0 \
            -columnspan 1 -rowspan 1 -sticky "snew"
    grid config $s.main    -column 0 -row 1 \
            -columnspan 1 -rowspan 1 -sticky "snew"

#    set slidetag_ 1
    if { $slidetag_ == 1 } {
        grid config $s.slider  -column 0 -row 2 \
            -columnspan 1 -rowspan 1 -sticky "snew"

        grid config $s.status  -column 0 -row 3 \
            -columnspan 1 -rowspan 1 -sticky "snew"

    } else {
	
        grid config $s.status  -column 0 -row 2 \
            -columnspan 1 -rowspan 1 -sticky "snew"
    }

    #
    # Set up grid for resizing.
    #
    # Column 0 (the only one) gets the extra space.
    grid columnconfigure $s 0 -weight 1
    # Row 2 (the main area) gets the extra space.
    grid rowconfigure $s 1 -weight 1

}

NamgraphView instproc RefreshWin { indexname } {

#    $self tkvar $indexname
#    $self instvar modelobj_
 
    $self instvar current_win_
    
    $self view_callback $current_win_

#    set index_ [set $indexname]
#    set plotdata [$modelobj_ set dataname($indexname)]

#    if { $index_ == 1 } {
# 	puts "plot data - $plotdata"
#    } else {
#	puts "delete data - $plotdata"
#    }
}

NamgraphView instproc build.c0 { w } {
    $self instvar current_win_ $w
    set current_win_ $w
     
    bind $w <Configure> "$self view_callback $w"
}   


NamgraphView instproc view_callback { w } {

    $self instvar modelobj_ slidetag_ 
    $self instvar minx_ maxx_ miny_ maxy_

    $w delete all

    foreach menuindex_ [$modelobj_ array names dataname] {
        $self tkvar $menuindex_
	set menuvalue [set $menuindex_]
	if { $menuvalue == 1 } {
	    set plotx [$modelobj_ set dataplotx($menuindex_)]
	    set ploty [$modelobj_ set dataploty($menuindex_)]
	    set plotcolor [$modelobj_ set plotcolor($menuindex_)]
	    set plotmark [$modelobj_ set plotmark($menuindex_)]

	    set cnt 0
	    foreach xvalue $plotx {
		if { $xvalue > $minx_ && $xvalue < $maxx_ } {
		    $self plot_xy $w $xvalue \
			[lindex $ploty $cnt] $minx_ $maxx_ \
			$miny_ $maxy_ $plotcolor \
			$plotmark &slidetag_
		}
		incr cnt
	    }
	}
    }

    $self draw_scale_xy 0
}

NamgraphView instproc plot_xy { c x y minx maxx miny maxy colorname markname traceflag} {
    $self instvar timeslider

#    if { $x<$minx || $x>$maxx || $y<$miny || $y>$maxy } {
#        $self draw_traceline $c $now $minx $maxx
#        return
#    }   

    set scrxr [winfo width $c]
    set scryb [winfo height $c] 

    #adjust the width
    set scrxr [expr $scrxr-$timeslider(swidth)]

    set yloc [expr $scryb-(($y-$miny)*$scryb/($maxy-$miny))]
    set xloc [expr $timeslider(swidth)/2 + (($x-$minx)*$scrxr/($maxx-$minx))]

    $c create bitmap $xloc $yloc -bitmap "$markname" -tag clickable \
        -foreground $colorname

#    if {$traceflag==1} {
#        $self draw_traceline $c $x $minx $maxx
#    }
}

NamgraphView instproc draw_traceline { t } {
    # draw trace line
    $self instvar timeslider minx_ maxx_ current_win_ 
    $self tkvar traceline

    set c $current_win_

    if { ($t < $minx_ ) || ($t > $maxx_) } {
        $c delete traceline
        return
    }
    
    set scrxl [expr $timeslider(swidth)/2]
    set scryt 0
    
    set scrxr [winfo width $c]
    set scryb [winfo height $c]
    set scrxr [expr $scrxr-$timeslider(swidth)]

    #redraw the data on the screen 
    
    set traceline_x [expr $scrxl + ($t-$minx_)*$scrxr/($maxx_-$minx_)]
    $c delete traceline
    $c addtag traceline withtag \
        [$c create line $traceline_x 0 $traceline_x $scryb -fill red]
    
}

NamgraphView instproc startclick {w x y} {
    $self instvar draglocation
    
    catch {unset draglocation}
    set draglocation(obj) [$w find closest $x $y]
    
    set draglocation(origx) $x
    set draglocation(origy) $y
}   
    
    
# set time to the current point
NamgraphView instproc endclick { w x y } {
    
    $self instvar draglocation timeslider maxx_ minx_ modelobj_
    
    if { $x == $draglocation(origx) && $y == $draglocation(origy)} {
    
        #decide current time

        set width [winfo width $w]

        #adjust the width
        set width [expr $width-$timeslider(swidth)]
        set x [expr $x-$timeslider(swidth)/2]
        set t [expr $minx_+($maxx_-$minx_)*$x/$width]

        # user control part, good solution ?

 	set animator [$modelobj_ animator]
	
        $animator settime $t
    }
    
}

#show current packet info
NamgraphView instproc showclick { w x y } {

    set z [$self viewloc_proc $w $x $y]
    set show_msg "Packet # [lindex $z 1] at time [lindex $z 0]"

    catch {destroy $w.f}
    frame $w.f -relief groove -borderwidth 2
    message $w.f.msg -width 8c -text $show_msg
    pack $w.f.msg
    pack $w.f
    catch {place_frame $w $w.f [expr $x+10] [expr $y+10]}
}

NamgraphView instproc endshow { w x y } {
    catch {destroy $w.f}
}

#Utilities: obj location processing

NamgraphView instproc viewloc_proc  { w x y } {

    $self instvar timeslider \
	  minx_ maxx_ miny_ maxy_
    #map the x,y to time and seqno range

    set width [winfo width $w]
    set height [winfo height $w]

    #adjust the width
    set width [expr $width-$timeslider(swidth)]
    set x [expr $x-$timeslider(swidth)/2]

    set x [expr $minx_+($maxx_-$minx_)*$x/$width]
    set y  [expr $miny_+1.0*($maxy_-$miny_)*($height-$y)/$height]

    set z [format "%7.5f %10.0f" $x $y ]
    return $z
}

#define zoom space 
    
NamgraphView instproc startrect {w x y} { 
    $self instvar rectlocation

    catch {unset rectlocation} 
    
    set rectlocation(xorig) $x
    set rectlocation(yorig) $y
    set tx [expr $x + 1] 
    set ty [expr $y + 1]
    set rectlocation(obj) \
        [$w create rect $x $y $tx $ty -outline gainsboro]
}
    
NamgraphView instproc dragrect {w x y} {
    $self instvar rectlocation
    $w delete $rectlocation(obj)
    set rectlocation(obj) \
        [$w create rect $rectlocation(xorig) $rectlocation(yorig) \
                $x $y -outline gainsboro]
}   

NamgraphView instproc endrect {w x y} {
    $self instvar timeslider title_ modelobj_ rectlocation
    $self instvar minx_ maxx_ miny_ maxy_

    $w delete $rectlocation(obj)

    if { $x == $rectlocation(xorig) || $y == $rectlocation(yorig)} {return}

    #map the x,y to time and seqno range

    set width [winfo width $w]
    set height [winfo height $w]
 
    #adjust the width
    set width [expr $width-$timeslider(swidth)]
    set x [expr $x-$timeslider(swidth)/2]
    set rectlocation(xorig) [expr $rectlocation(xorig)-$timeslider(swidth)/2]

    # XXX adjust the value

    set timestart [expr $minx_+($maxx_-$minx_)*$rectlocation(xorig)/$width]
    set timeend   [expr $minx_+($maxx_-$minx_)*$x/$width]
    set seqstart  [expr $miny_+1.0*($maxy_-$miny_)* \
    		   ($height-$rectlocation(yorig))/$height]
    set seqend    [expr $miny_+1.0*($maxy_-$miny_)*($height-$y)/$height]

    if { $timestart > $timeend } {
	set tmpt $timestart
	set timestart $timeend
	set timeend $tmpt
    }

    if { $seqstart > $seqend } {
	set tmpt $seqstart
	set seqstart $seqend
	set seqend $tmpt
    }

    #create a new NamgraphView (zoomed)
    set zoomview [new NamgraphView $title_ $modelobj_ $timestart \
  		  $timeend $seqstart $seqend "ZOOM"]
    $modelobj_ addObserver $zoomview

}

NamgraphView instproc startmove {w o x y} {
    $self instvar rectlocation
    scan [$w coords $o] "%f %f %f %f" x1 y1 x2 y2
    set rectlocation(x) [expr abs($x - $x1)]
    set rectlocation(y) [expr abs($y - $y1)]
}

NamgraphView instproc moverect {w o x y} {
    $self instvar rectlocation
    scan [$w coords $o] "%f %f %f %f" x1 y1 x2 y2
    set dx [expr $x - $x1 - $rectlocation(x)]
    set dy [expr $y - $y1 - $rectlocation(y)]
    $w move $o $dx $dy
}

#
# mode = 0:	Draw scale in free mode
# mode = 1:	Draw scale in integer (fully) mode
# mode = 2:	Draw scale in integer (evenly) mode
#
NamgraphView instproc draw_scale_xy { mode } {

    $self instvar timeslider minx_ maxx_ miny_ maxy_ current_win_
    $self instvar modelobj_

    set c $current_win_

    set scrxl [expr $timeslider(swidth)/2]
    set scryt 0

    set scrxr [winfo width $c]
    set scryb [winfo height $c]

    #adjust the width
    set scrxr [expr $scrxr-$timeslider(swidth)]

    set nxpoints 8
    set nypoints 6

    set yincr [expr ($maxy_-$miny_)/$nypoints.0]
    set xincr [expr ($maxx_-$minx_)/$nxpoints.0]

    for {set i 1} {$i<$nypoints} {incr i} {
        #set ypos [expr $scryb-($scryb/$nypoints.0)*$i]
	set yval [expr $miny_+$i*$yincr]
	set yval [expr int($yval)]
        set ypos [expr $scryb-($scryb*($yval-$miny_)/($maxy_-$miny_))]
	set yval [$modelobj_ verifyymark $yval]

        #set yval [format "%+15.5f" [expr $miny_+$i*$yincr]]
        $c create line $scrxl $ypos [expr $scrxl+10] $ypos -fill #6725a0
        $c create text [expr $scrxl+15] [expr $ypos-7] -text $yval -anchor nw \
            -justify left -fill #6725a0 \
            -font "-adobe-new century schoolbook-medium-r-normal--10-100-75-75-p-60-iso8859-1"
   }

    for {set i 1} {$i<$nxpoints} {incr i} {
        set xpos [expr ($scrxr/$nxpoints.0)*$i + $scrxl]
        set xval [format "%+7.5f" [expr $minx_+($i*$xincr)]]
        $c create line $xpos 0 $xpos 10 -fill #6725a0
        $c create text  $xpos 15 -text $xval -anchor n\
            -justify left -fill #6725a0 \
        -font "-adobe-new century schoolbook-medium-r-normal--10-100-75-75-p-60-iso8859-1"
   }
}

#-----------------------------------------------------------------------
# NamgraphView instproc update { time }
#   - extracts the time value from a nam event
#     and updates the trace line on a namGraph to that point 
#
#-----------------------------------------------------------------------
NamgraphView instproc update { time } {

    $self instvar now_ winname_ modelobj_

    if ![winfo exists $winname_ ] {
	$modelobj_ deleteObserver $self
	$self destroy
	return
    }

    if { [string compare $time "*"] != 0 } {
	set now_ $time
        $self draw_traceline $now_
    }
}














