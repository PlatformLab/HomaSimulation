# slow start mechanism with some features
# such as labeling, annotation, nam-graph, and window size monitoring

set ofile "tcp-sack-unfair"
set tcptype "Agent/TCP/Sack1"
set sinktype "Agent/TCPSink/Sack1"
Agent/TCP set window_ 200

set L1rate "10Mb"
set L1del "5ms"
set L1type "DropTail"

set L2rate "1.5Mb"
set L2del "40ms"
set L2type "DropTail"
set L2qlim 5

set ftpstart 0.0001
set cbrstart 2.80
set cbrrate 0.0055
set endtime 5.0

source tcp-common.tcl

$ns at 0.589653 "$ns trace-annotate \"0.589653 Fast RTX: cwnd:(24 -> 12), ssth: 12\""
$ns at 1.024162 "$ns trace-annotate \"1.024162 Fast RTX: cwnd:(12 -> 6), ssth: 6\""
$ns at 1.158039 "$ns trace-annotate \"1.158039 CA: cwnd:6.17\""

$ns at 3.022799 "$ns trace-annotate \"3.022799 Fast RTX: cwnd:(21.9 -> 10), ssth: 10\""
$ns at 3.262799 "$ns trace-annotate \"3.262799 Fast RTX: cwnd:(10 -> 5), ssth: 5\""
$ns at 3.646743 "$ns trace-annotate \"3.646743 CA: cwnd:5.2\""
$ns at 3.774854 "$ns trace-annotate \"3.774854 Fast RTX: cwnd:(5.2 -> 2), ssth: 2\""
$ns at 4.030743 "$ns trace-annotate \"4.030743 CA: cwnd:2.5\""
$ns at 4.676077 "$ns trace-annotate \"4.676077 Slow-start/FRTX: cwnd:(3.5 -> 1), ssth: 2\""
$ns at 4.910743 "$ns trace-annotate \"4.910743 CA: cwnd:2.5\""

$ns run
