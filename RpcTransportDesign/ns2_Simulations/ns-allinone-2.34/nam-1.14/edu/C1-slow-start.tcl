# Slow start protocol in a heavily loaded network.
# features : labeling, annotation, nam-graph, and window size monitoring

#
#	n0 			  n5 
#	   \	  	        / 
#	n1 -- n3 ---------- n4 -- n6
#	   /			\ 
#	n2 			  n7


set ns [new Simulator]

$ns color 0 black
$ns color 1 red

$ns trace-all [open C1-slow-start.tr w]
$ns namtrace-all [open C1-slow-start.nam w]

### build topology with 8 nodes

	foreach i " 0 1 2 3 4 5 6 7" {
		set n$i [$ns node]
	}

        $ns at 0.0 "$n0 label TCP"
        $ns at 0.0 "$n5 label TCP"
        $ns at 0.0 "$n1 label CBR-1"
        $ns at 0.0 "$n2 label CBR-2" 
        $ns at 0.0 "$n6 label CBR-1"
        $ns at 0.0 "$n7 label CBR-2"

        $ns duplex-link $n0 $n3 5Mb 20ms DropTail
        $ns duplex-link $n1 $n3 1Mb 20ms DropTail
        $ns duplex-link $n2 $n3 1Mb 20ms DropTail
        $ns duplex-link $n3 $n4 1Mb 50ms DropTail
	$ns duplex-link $n4 $n5 5Mb 20ms DropTail
        $ns duplex-link $n4 $n6 1Mb 20ms DropTail
        $ns duplex-link $n4 $n7 1Mb 20ms DropTail

        $ns queue-limit $n3 $n4 10

        $ns duplex-link-op $n0 $n3 orient right-down
        $ns duplex-link-op $n1 $n3 orient right
        $ns duplex-link-op $n2 $n3 orient right-up
        $ns duplex-link-op $n3 $n4 orient right     
        $ns duplex-link-op $n4 $n5 orient right-up
        $ns duplex-link-op $n4 $n6 orient right     
        $ns duplex-link-op $n4 $n7 orient right-down     

        $ns duplex-link-op $n3 $n4 queuePos 0.5

Agent/TCP set nam_tracevar_ true         
Agent/TCP set window_ 20

### TCP between n0 and n5 (Black)

set tcp [new Agent/TCP]
$tcp set fid_ 0
$ns attach-agent $n0 $tcp
        
set sink [new Agent/TCPSink]
$ns attach-agent $n5 $sink

$ns connect $tcp $sink

set ftp [new Application/FTP]
$ftp attach-agent $tcp

$ns add-agent-trace $tcp tcp
$ns monitor-agent-trace $tcp
$tcp tracevar cwnd_
$tcp tracevar ssthresh_

### CBR traffic between (n1 & n6) and (n2 & n7)

set cbr0 [new Agent/CBR]
$ns attach-agent $n1 $cbr0
$cbr0 set fid_ 1
$cbr0 set packetSize_ 500
$cbr0 set interval_ 0.01

set null0 [new Agent/CBR]
$ns attach-agent $n6 $null0

$ns connect $cbr0 $null0

set cbr1 [new Agent/CBR]
$ns attach-agent $n2 $cbr1
$cbr1 set fid_ 1
$cbr1 set packetSize_ 1000
$cbr1 set interval_ 0.01

set null1 [new Agent/CBR]
$ns attach-agent $n7 $null1

$ns connect $cbr1 $null1 

proc finish {} {

        global ns
        $ns flush-trace

        puts "filtering..."
        exec tclsh ../bin/namfilter.tcl C1-slow-start.nam
        puts "running nam..."
        exec nam C1-slow-start.nam &
        exit 0
}

### set operations
$ns at 0.05 "$cbr0 start"
$ns at 2.3 "$cbr0 stop" 
$ns at 0.1 "$cbr1 start"
$ns at 2.5 "$cbr1 stop"
$ns at 0.5 "$ftp start"
$ns at 2.5 "$ftp stop"
$ns at 2.7 "finish"

$ns at 0.50 "$cbr1 set interval_ 0.02"
$ns at 1.50 "$cbr0 set interval_ 0.02"
$ns at 1.70 "$cbr1 set interval_ 0.01"
$ns at 1.70 "$cbr1 set interval_ 0.01"

### add annotations
$ns at 0.0 "$ns trace-annotate \"TCP in a heavely loaded network\"" 
$ns at 0.05 "$ns trace-annotate \"CBR-1 starts\""
$ns at 0.1 "$ns trace-annotate \"CBR-2 starts\""
$ns at 0.5 "$ns trace-annotate \"TCP starts\""

$ns at 0.5 "$ns trace-annotate \"Send Packet_0 : Initial window size = 1\""
$ns at 0.66 "$ns trace-annotate \"Ack_0\""
$ns at 0.75 "$ns trace-annotate \"Send Packet_1,2 : Increase window size to 2\""
$ns at 0.86 "$ns trace-annotate \"Ack_1,2\""
$ns at 0.95 "$ns trace-annotate \"Send Packet_3,4,5,6 : Increase window size to 4\""
$ns at 1.05 "$ns trace-annotate \"Ack_3,4,5,6\""
$ns at 1.13 "$ns trace-annotate \"Send Packet_7 to 14 : Increase window size to 8\""
$ns at 1.24 "$ns trace-annotate \"Ack_7 to 14 \""
$ns at 1.32 "$ns trace-annotate \"Send Packet_15 to 30 : Increase window size to 16\""
$ns at 1.41 "$ns trace-annotate \"Lost Packet_21,22,23,26,27,28,29,30 !\""

$ns at 1.46 "$ns trace-annotate \"8 Ack_20s \""
$ns at 1.55 "$ns trace-annotate \"Send Packet_31 to 39\""
$ns at 1.67 "$ns trace-annotate \"8 Ack_20s \""

$ns at 1.63 "$ns trace-annotate \"Retransmit Packet_21 : SLOW START !\""
$ns at 1.80 "$ns trace-annotate \"Ack_21 \""
$ns at 1.89 "$ns trace-annotate \"Send Packet_22,23 : window size 2\""
$ns at 2.01 "$ns trace-annotate \"2 Ack_25s \""
$ns at 2.11 "$ns trace-annotate \"Send Packet_26,27,28 : window size 3\""
$ns at 2.13 "$ns trace-annotate \"Lost Packet_27,28 !\""
$ns at 2.26 "$ns trace-annotate \"Ack_26 \""
$ns at 2.35 "$ns trace-annotate \"Send Packet_27,28,29,30 : window size 4\""
$ns at 2.50 "$ns trace-annotate \"Ack_40 \""

$ns at 2.55 "$ns trace-annotate \"FTP stops\""

$ns run


