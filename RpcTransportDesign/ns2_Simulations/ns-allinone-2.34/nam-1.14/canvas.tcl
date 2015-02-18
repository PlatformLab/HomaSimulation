puts "sourcing canvas.tcl"

set oblist ""

proc fillrectangle {x y x2 y2 col t} {
    global canv oblist
    set id [$canv create restangle $x $y $x2 $y2 -fill $col]
    if {$t==1} {
	lappend oblist $id
    }
}

proc drawoval {x y x2 y2 col t} {
    global canv oblist
    set id [$canv create restangle $x $y $x2 $y2]
    if {$t==1} {
	lappend oblist $id
    }
}

proc drawpolygon args {
    global canv oblist
    set col [lindex $args 0]
    set t [lindex $args 1]
    set id [eval "$canv create polygon [lrange $args 2 end] -outline $col -fill \"\""]
    if {$t==1} {
        lappend oblist $id
    }
}

proc fillpolygon args {
    global canv oblist
    set col [lindex $args 0]
    set t [lindex $args 1]
    set id [eval "$canv create polygon [lrange $args 2 end] -outline $col"]
    if {$t==1} {
        lappend oblist $id
    }
}

proc drawstring {font str x y t} {
    global canv oblist
    set id [$canv create text $x $y -text $str]
    if {$t==1} {
        lappend oblist $id
    }
}

proc drawline {x y x2 y2 col t} {
    global canv oblist
    set id [$canv create line $x $y $x2 $y2]
    if {$t==1} {
        lappend oblist $id
    }
}

proc update_display {} {
    update
}

proc clearobjs {} {
    global canv oblist
    foreach obj $oblist {
	$canv delete $obj
    }
}