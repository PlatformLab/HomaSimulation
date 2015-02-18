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

Animator instproc take_snapshot {} {
		$self instvar netModel
		$self testsuite_view $netModel nam
}

Animator instproc testsuite_view { viewobject windowname } {
		puts "take a snapshot here. "
		$viewobject testview testview
}

Animator instproc playing_backward {} {
		$self instvar netView running backward
		focus $netView
		if { $backward == 0 } {
				$self set_backward_dir 1
				if { $running != 1 } {
						$self play 1
						$self renderFrame
				}
		}
		set backward 1
}

Animator instproc playing_forward {} {
		$self instvar netView running direction
		focus $netView
		if { $direction == -1} {
				$self set_forward_dir 1
				if { $running != 1 } {
						$self play 1
						$self renderFrame
				}
		}
}

Animator instproc terminating_nam {} {
		destroy .
}







