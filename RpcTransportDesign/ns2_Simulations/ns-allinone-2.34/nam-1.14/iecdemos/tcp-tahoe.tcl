# slow start mechanism with some features
# such as labeling, annotation, nam-graph, and window size monitoring

set ofile "tcp-tahoe"
set tcptype "Agent/TCP"
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

$ns at 0.659632 "$ns trace-annotate \"0.659632 Slow-start: cwnd:(30 -> 1), ssth:15\""
$ns at 1.344965 "$ns trace-annotate \"1.344965 CA: cwnd: 15\""
$ns run
