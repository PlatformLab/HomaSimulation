# slow start mechanism with some features
# such as labeling, annotation, nam-graph, and window size monitoring

set ofile "tcp-sack-drr"
set tcptype "Agent/TCP/Sack1"
set sinktype "Agent/TCPSink/Sack1"
Agent/TCP set window_ 200

set L1rate "10Mb"
set L1del "5ms"
set L1type "DropTail"

set L2rate "1.5Mb"
set L2del "40ms"
set L2type "DRR"
set L2qlim 5
set blimit [expr $L2qlim * [Agent/TCP set packetSize_]]

set ftpstart 0.0001
set cbrstart 2.80
set cbrrate 0.0055
set endtime 5.0

source tcp-common.tcl

$ns at 0.573653 "$ns trace-annotate \"0.573653 Fast RTX: cwnd:(21 -> 10), ssth: 10\""
$ns at 0.777408  "$ns trace-annotate \"0.777408 CA: cwnd:10.1\""

$ns at 3.013491 "$ns trace-annotate \"3.013491 Fast RTX: cwnd:(13.2 -> 6), ssth: 6\""
$ns at 3.247991 "$ns trace-annotate \"3.247991 CA: cwnd:6.17\""

$ns at 4.298713 "$ns trace-annotate \"4.298713 Fast RTX: cwnd:(13.8 -> 6), ssth: 6\""
$ns at 4.463991 "$ns trace-annotate \"4.463991 CA: cwnd:6.17\""

$ns at 5.488047 "$ns trace-annotate \"5.488047 Fast RTX: cwnd:(13.8 -> 6), ssth: 6\""

$ns run
