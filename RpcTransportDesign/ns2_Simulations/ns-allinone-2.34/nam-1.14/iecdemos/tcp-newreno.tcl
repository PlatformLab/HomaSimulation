# slow start mechanism with some features
# such as labeling, annotation, nam-graph, and window size monitoring

set ofile "tcp-newreno"
set tcptype "Agent/TCP/Newreno"
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

$ns at 0.659632 "$ns trace-annotate \"0.659632 Fast RTX: cwnd:(30 -> 15), ssth: 15\""
$ns at 1.443387 "$ns trace-annotate \"1.443387 CA: cwnd:15.07\""
$ns at 1.618544 "$ns trace-annotate \"1.618544 Fast RTX: cwnd:(15.97 -> 7), ssth: 7\""
$ns at 1.841211 "$ns trace-annotate \"1.841211 CA: cwnd:7.14\""

$ns run
