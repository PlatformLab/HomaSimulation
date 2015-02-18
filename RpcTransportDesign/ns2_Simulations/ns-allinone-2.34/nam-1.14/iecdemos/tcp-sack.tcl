# slow start mechanism with some features
# such as labeling, annotation, nam-graph, and window size monitoring

set ofile "tcp-sack"
set tcptype "Agent/TCP/Sack1"
set sinktype "Agent/TCPSink/Sack1"
Agent/TCP set window_ 200

set L1rate "10Mb"
set L1del "5ms"
set L1type "DropTail"

set L2rate "1.5Mb"
set L2del "40ms"
set L2type "DropTail"
set L2qlim 10

set ftpstart 0.001
set cbrstart 0.002

source tcp-common.tcl

$ns at 0.659687 "$ns trace-annotate \"0.659687 Fast RTX: cwnd:(30 -> 15), ssth: 15\""
$ns at 0.964942 "$ns trace-annotate \"0.964942 CA: cwnd:15.07\""

$ns run
