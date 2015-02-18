# Sample nam configuration file
# 
# At start, nam will automatically load .nam.tcl in current directory,
# if there is such a file.
#
# $Header: /cvsroot/nsnam/nam-1/ex/sample.nam.tcl,v 1.3 1999/02/16 21:21:13 haoboy Exp $

# Parameters below are for automatic layout only. To turn on automatic layout, 
# do *not* give any link orientation information in your ns script when you 
# create your links. To turn it off, set those orientation information.

# KCa_: attractive force constant
# KCr_: repulsive force constant
#
# Here is a method to layout a large topology (say, 100 nodes). First set 
# KCa_ and KCr_ to 0.2, do about 30 iterations (i.e., after start, set 
# the 'Iterations' input box to 30), then set KCr_ to 1.0, KCa_ to about 0.005,
# then do about 10 iterations, then set KCa_ to 0.5, KCr_ to 1.0, do about 6 
# iterations.
NetworkModel/Auto set KCa_ 0.20
NetworkModel/Auto set KCr_ 0.20

# Seed to generate initial random layout.
NetworkModel/Auto set RANDOM_SEED_ 1

# Layout iterations done during startup
NetworkModel/Auto set AUTO_ITERATIONS_ 10

# Layout iterations done each time the 'relayout' button is pressed or Enter 
# is clicked in the input boxes.
NetworkModel/Auto set INCR_ITERATIONS_ 10
