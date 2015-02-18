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
# initialize layout constant

NetworkModel/Wireless set Wpxmax_ 4
NetworkModel/Wireless set Wpymax_ 4

NetworkModel/Wireless instproc init { animator tracefile } {
	eval $self next $animator $tracefile
	NetworkModel/Wireless instvar Wpxmax_ Wpymax_
	$self set_range $Wpxmax_ $Wpymax_
}

NetworkModel/Wireless instproc set_range { width height } {
	$self instvar Wpxmax_ Wpymax_
	set Wpxmax_ $width
	set Wpymax_ $height
}

NetworkModel/Wireless instproc add_link { e } {

	set val [lrange $e 1 end]

	$self layout_link $val

}

