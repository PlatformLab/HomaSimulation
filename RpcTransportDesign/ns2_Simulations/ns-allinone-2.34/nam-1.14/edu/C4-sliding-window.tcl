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

#$ns trace-all [open C4-sliding-window.tr w]
#$ns namtrace-all [open C4-sliding-window.nam w]

### build topology with 8 nodes

	foreach i " 0 1 2 3 4 5 6 7" {
		set n$i [$ns node]
	}

        $ns at 0.0 "$n0 label SLIDING"
        $ns at 0.0 "$n5 label SLIDING"
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

        $ns queue-limit $n3 $n4 15

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
Agent/TCP set maxcwnd_ 8

### TCP between n0 and n5 (Black)

#set sliding [new Agent/TCP/SlidingWindow]
set sliding [new Agent/TCP]
$sliding set fid_ 0
$ns attach-agent $n0 $sliding
        
#set sink [new Agent/TCPSink/SlidingWindowSink]
set sink [new Agent/TCPSink]
$ns attach-agent $n5 $sink

$ns connect $sliding $sink

set ftp [new Application/FTP]
$ftp attach-agent $sliding

$ns add-agent-trace $sliding tcp
$ns monitor-agent-trace $sliding
$sliding tracevar cwnd_
$sliding tracevar ssthresh_

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
$cbr1 set interval_ 0.02

set null1 [new Agent/CBR]
$ns attach-agent $n7 $null1

$ns connect $cbr1 $null1 

proc finish {} {

        global ns
        $ns flush-trace

#        puts "filtering..."
#        exec tclsh ../bin/namfilter.tcl C4-sliding-window.nam
        puts "running nam..."
        exec nam C4-sliding-window.nam &
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

### add annotations

$ns at 0.05 "$ns trace-annotate \"CBR-1 starts\""
$ns at 0.1 "$ns trace-annotate \"CBR-2 starts\""
$ns at 0.5 "$ns trace-annotate \"FTP starts\""
$ns at 2.55 "$ns trace-annotate \"FTP stops\""

$ns run


