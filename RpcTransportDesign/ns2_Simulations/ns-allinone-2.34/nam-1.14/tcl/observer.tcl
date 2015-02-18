#
# Inspiried by Java java.util.Observer
# for MCV

Class Observer
Observer set uniqueID_ 0

Observer proc getid {} {
    set id [Observer set uniqueID_]
    Observer set uniqueID_ [expr $id + 1]
    return $id
}

Observer instproc init {} {

    $self instvar id_
    set id_ [Observer getid]
}

Observer instproc id {} {
    $self instvar id_
    return $id_
}

# Needs to be overwritten

Observer instproc update {} {

}
