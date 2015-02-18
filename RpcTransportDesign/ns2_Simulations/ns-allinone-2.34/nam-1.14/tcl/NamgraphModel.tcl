Class NamgraphModel -superclass Observable

NamgraphModel instproc init { nid obs } {

    $self next

    $self instvar id_ datacount Maxy_ Maxx_ animator_id_ timeslider_swidth_
    set id_ $nid
    set datacount 0
    set animator_id_ $obs

    set Maxy_ [$obs set highest_seq($id_)]
    set Maxx_ [$obs set maxtime]

}

NamgraphModel instproc id {} {
    $self instvar id_

    return $id_

}

NamgraphModel instproc animator {} {

    $self instvar animator_id_
    return $animator_id_

}

NamgraphModel instproc adddata { ob mid x y ymark } {

    $self instvar dataname datacount id_ dataplotx dataploty
    $self instvar plotmark plotcolor dataplotym

    $self tkvar midbuilder
    
    if ![info exists midbuilder($mid)] {
	set midbuilder($mid) $datacount

	set mname [$ob set filter_id($mid)]
	set pmark [$ob set plotmark($id_.$mid)]
	set pcolor [$ob set plotcolor($id_.$mid)]
	set pcolor [$ob set colorname($pcolor)]

        set dataname($datacount) $mname 
	set plotmark($datacount) $pmark
	set plotcolor($datacount) $pcolor
	incr datacount
    }

    set current_index $midbuilder($mid)
    
    lappend dataplotx($current_index) $x
    lappend dataploty($current_index) $y
    set dataplotym($y) $ymark

}

NamgraphModel instproc verifyymark { index } {

    $self instvar dataplotym

    if [info exists dataplotym($index)] {

	return $dataplotym($index)

    }

    return ""

}

NamgraphModel instproc startview {title} {

    $self instvar Maxy_ Maxx_

    set vob [new NamgraphView $title $self 0 $Maxx_ 0 $Maxy_ 1]
    $self addObserver $vob
}

#----------------------------------------------------------------------
# NamgraphModel instproc update { arg } 
#  - Must have update 
#  - two possible args
#      1. Animator current time - single value
#      2. trace event - a list 
#
#  - Requires that all trace events have a -t flag with a value
#----------------------------------------------------------------------
NamgraphModel instproc update { arg } {

  # Check to see if it is a trace event
  set now [get_trace_item "-t" $arg]
  if { $now == "" } {
    # no -t flag so assume it must be a single value
    set now $arg
    $self notifyObservers $now 
  } else {
    # arg is trace event, what to do ?
  }
}
