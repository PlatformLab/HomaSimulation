#
# Copyright (C) 2001 by USC/ISI
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


#-----------------------------------------------------------------------
# Animator instproc node_tclscript {node_id button_label tcl_command}
#  - calls set_node_tclscript which adds a button to a node's get info
#    window 
#-----------------------------------------------------------------------
Animator instproc node_tclscript {node_id button_label tcl_command} {
  $self instvar netModel
  $netModel set_node_tclscript $node_id $button_label $tcl_command
}
