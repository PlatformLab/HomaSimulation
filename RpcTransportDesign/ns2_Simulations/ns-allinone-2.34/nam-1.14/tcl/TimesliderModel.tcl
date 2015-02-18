Class TimesliderModel -superclass Observable

TimesliderModel instproc init { min_t max_t curr_t aid} {
    $self next

    $self instvar mint_ maxt_ currt_ aid_

    set mint_ $min_t
    set maxt_ $max_t
    set currt_ $curr_t
    set aid_ $aid

}

TimesliderModel instproc setmintime {min_t} {
    $self instvar mint_
    set mint_ $min_t
    set e [list $mint_ min]
    $self notifyObservers $e
}

TimesliderModel instproc setmaxtime {max_t} {
    $self instvar maxt_
    set maxt_ $max_t
    set e [list $maxt_ max]
    $self notifyObservers $e
}

TimesliderModel instproc setcurrenttime {curr_t} {
    $self instvar currt_
    set currt_ $curr_t
    set e [list $curr_t now]
    $self notifyObservers $e
}

TimesliderModel instproc getmintime {} {
    $self instvar mint_
    return $mint_
}

TimesliderModel instproc getmaxtime {} {
    $self instvar maxt_
    return $maxt_
}

TimesliderModel instproc getcurrenttime {} {
    $self instvar currt_
    return $currt_
}   

TimesliderModel instproc getanimator {} {
    $self instvar aid_
    return $aid_
}  

TimesliderModel instproc setpipemode { p } {
    $self instvar pipemode_
    set pipemode_ $p
}  

TimesliderModel instproc getpipemode { } {
    $self instvar pipemode_
    return $pipemode_
}  

