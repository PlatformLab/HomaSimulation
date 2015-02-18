# slow start mechanism with some features
# such as labeling, annotation, nam-graph, and window size monitoring

set ofile "tcp-reno"
set tcptype "Agent/TCP/Reno"
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
$ns at 0.787632 "$ns trace-annotate \"0.787632 CA: cwnd:15.07\""
$ns at 0.862843 "$ns trace-annotate \"0.862843 Fast RTX: cwnd:(15.07 -> 7), ssth: 7\""
$ns at 1.274544 "$ns trace-annotate \"1.274544 Slow-Start: cwnd:(7.14 -> 1), ssth: 3\""
$ns at 1.596176 "$ns trace-annotate \"1.596176 CA: cwnd:3.33\""
$ns run
