# slow start mechanism with some features
# such as labeling, annotation, nam-graph, and window size monitoring

set ofile "tcp-reno-ecn"
set tcptype "Agent/TCP/Reno"
Agent/TCP set window_ 200

# these are fairly harsh (unrecommended) RED
# parameters, but illustrate the point quickly
# for our contrived, underbuffered network
Queue/RED set thresh_ 1
Queue/RED set maxthresh_ 4
Queue/RED set linterm_ 1
Queue/RED set q_weight_ 0.8
Queue/RED set wait_ false
Queue/RED set mean_pktsize_ 1000

set L1rate "10Mb"
set L1del "5ms"
set L1type "DropTail"

set L2rate "1.5Mb"
set L2del "40ms"
set L2type "RED"
set L2qlim 10

set ecn true

set ftpstart 0.001
set cbrstart 0.002

source tcp-common.tcl

$ns at 0.455877 "$ns trace-annotate \"0.455877 Fast RTX: cwnd:(12 -> 6), ssth: 6\""
$ns at 0.659632 "$ns trace-annotate \"0.659632 CA: cwnd:6.17\""

$ns at 1.131632 "$ns trace-annotate \"1.131632 Fast RTX: cwnd:(10.13 -> 5), ssth: 5\""
$ns at 1.324720 "$ns trace-annotate \"1.324720 CA: cwnd:5.2\""

$ns run
