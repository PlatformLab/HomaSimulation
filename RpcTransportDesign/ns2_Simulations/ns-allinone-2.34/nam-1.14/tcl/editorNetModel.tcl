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

NetworkModel/Editor set Wpxmin_ 500.0
NetworkModel/Editor set Wpymin_ 500.0
NetworkModel/Editor set Wpxmax_ 625.0
NetworkModel/Editor set Wpymax_ 625.0

#NetworkModel/Editor set Wpxmin_ 0.0
#NetworkModel/Editor set Wpymin_ 0.0
#NetworkModel/Editor set Wpxmax_ 100.0
#NetworkModel/Editor set Wpymax_ 100.0

NetworkModel/Editor instproc init {animator tracefile} {
  # Tracefile maybe a space which indicates that
  # a new nam editor window is being created
  eval $self next $animator {$tracefile}
  NetworkModel/Editor instvar Wpxmax_ Wpymax_  Wpxmin_ Wpymin_
  $self set_range $Wpxmin_ $Wpymin_ $Wpxmax_ $Wpymax_
}

NetworkModel/Editor instproc set_range {xmin ymin xmax ymax } {
	$self instvar Wpxmin_ Wpymin_ Wpxmax_ Wpymax_
	set Wpxmin_ $xmin
	set Wpymin_ $ymin
	set Wpxmax_ $xmax
	set Wpymax_ $ymax
}


