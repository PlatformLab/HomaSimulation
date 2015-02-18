#
# for MCV
#

Class Observable

Observable instproc init {} {
    $self instvar observerlist_ 
    set observerlist_ ""
}

# Adds an observer to the set of observers for this object.
# @param   o   an observer to be added.

Observable instproc addObserver { o } {
    $self instvar observerlist_

    set cnt 0
    set oid [$o id]
    foreach ob $observerlist_ {
	set obid [$ob id]
	if { $oid == $obid } {
	    set cnt 1
	    break;
	}
    }  

    if { $cnt == 0 } {
        lappend observerlist_ $o
    }

}

# Deletes an observer from the set of observers of this object.
# @param   o   the observer to be deleted.

Observable instproc  deleteObserver { o } {

    $self instvar observerlist_
    set backlist_ ""
    set oid [$o id]
    foreach ob $observerlist_ {
        set obid [$ob id]
        if { $oid != $obid } {
            lappend backlist_ $ob
        } else {
#	    $o destroy
#	    leave the work to the application
 	}
    }
    
    set observerlist_ $backlist_
}

# If this object has changed, as indicated by the
# hasChanged method, then notify all of its observers
# and then call the clearChanged method to indicate
# that this object has no longer changed.
# 
# Each observer has its update method called with two
# arguments: this observable object and the arg argument 

Observable instproc notifyObservers { arg } {

    $self instvar observerlist_

    #??? Synchronization here before updating ???

    foreach ob $observerlist_ {

	if ![ catch { $ob info class } ] {
		
	    $ob update $arg
        }   

    }
}

# Returns the number of observers of this object.

Observable instproc countObservers {} {
    $self instvar observerlist_
    set size [llength $observerlist_]
    return $size
}
