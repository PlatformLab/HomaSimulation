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
# $Header: /cvsroot/nsnam/nam-1/tcl/autoNetModel.tcl,v 1.4 2001/01/08 22:51:41 mehringe Exp $ 

# initialize layout constant
NetworkModel/Auto set RANDOM_SEED_ 1
NetworkModel/Auto set KCa_ 0.15
NetworkModel/Auto set KCr_ 0.75
NetworkModel/Auto set Recalc_ 1
NetworkModel/Auto set AUTO_ITERATIONS_ 10
NetworkModel/Auto set INCR_ITERATIONS_ 10

NetworkModel/Auto instproc init { animator tracefile } {
  eval $self next $animator $tracefile
  NetworkModel/Auto instvar INCR_ITERATIONS_ KCa_ KCr_
  $animator set-layout-params $INCR_ITERATIONS_ $KCa_ $KCr_
}

NetworkModel/Auto instproc do_relayout { Iter Kca Kcr Recalc } {
  $self instvar INCR_ITERATIONS_ KCa_ KCr_ Recalc_
  set KCa_ $Kca
  set KCr_ $Kcr
  set Recalc_ $Recalc
  set INCR_ITERATIONS_ $Iter
  $self relayout
}
