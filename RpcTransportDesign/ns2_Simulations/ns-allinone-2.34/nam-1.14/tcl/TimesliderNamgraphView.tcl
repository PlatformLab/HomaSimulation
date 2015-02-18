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

Class TimesliderNamgraphView -superclass TimesliderView

TimesliderNamgraphView instproc setattachedView {viewid} {
	$self instvar _attachedviewid

	set _attachedviewid $viewid

}

TimesliderNamgraphView instproc update {timeset} {

	$self next $timeset

        set time [lindex $timeset 0]
	set type [lindex $timeset 1]    

	if {[string compare $type "max"] == 0} {

	    $self instvar _attachedviewid
	    set s [$_attachedviewid set current_win_]
	    $_attachedviewid set maxx_ $time
	    $_attachedviewid view_callback $s

	}

}













