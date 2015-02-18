# Slow start protocol in a heavily loaded network.
#
#	n0 			  n5 
#	   \	  	        / 
#	n1 -- n3 ---------- n4 -- n6
#	   /			\ 
#	n2 			  n7


set ns [new Simulator]

$ns color 0 black
$ns color 1 red

$ns namtrace-all [open test-ptp-2.nam w]

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

$ns duplex-link $n0 $n3 1Mb 50ms DropTail
$ns duplex-link $n1 $n3 0.5Mb 50ms DropTail
$ns duplex-link $n2 $n3 0.5Mb 50ms DropTail
$ns duplex-link $n3 $n4 0.5Mb 100ms DropTail
$ns duplex-link $n4 $n5 1Mb 50ms DropTail
$ns duplex-link $n4 $n6 0.5Mb 50ms DropTail
$ns duplex-link $n4 $n7 0.5Mb 50ms DropTail

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
# set window size
Agent/TCP set maxcwnd_ 8

### TCP between n0 and n5 (Black)
set sliding [new Agent/TCP]
$sliding set fid_ 0
$ns attach-agent $n0 $sliding
        
set sink [new Agent/TCPSink]
$ns attach-agent $n5 $sink
$ns connect $sliding $sink

set ftp [new Application/FTP]
$ftp attach-agent $sliding

### CBR traffic between (n1 & n6) and (n2 & n7)
set cbr0 [new Agent/CBR]
$ns attach-agent $n1 $cbr0
$cbr0 set fid_ 1
$cbr0 set packetSize_ 500
$cbr0 set interval_ 0.02

set null0 [new Agent/CBR]
$ns attach-agent $n6 $null0

$ns connect $cbr0 $null0

set cbr1 [new Agent/CBR]
$ns attach-agent $n2 $cbr1
$cbr1 set fid_ 1
$cbr1 set packetSize_ 1000
$cbr1 set interval_ 0.03

set null1 [new Agent/CBR]
$ns attach-agent $n7 $null1

$ns connect $cbr1 $null1 

proc finish {} {
        global ns
        $ns flush-trace
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

### take snapshots
foreach i "0.0 0.5 1.0 1.5 1.8 1.9 2.0 2.1 2.2 2.3 2.4 2.5 2.6" {
$ns at $i "$ns snapshot"
}

### take snapshot operations
$ns at 1.8 "$ns re-rewind-nam"
$ns at 2.1 "$ns rewind-nam"
$ns at 2.65 "$ns terminate-nam"

### add annotations
$ns at 0.05 "$ns trace-annotate \"CBR-1 starts\""
$ns at 0.1 "$ns trace-annotate \"CBR-2 starts\""
$ns at 0.5 "$ns trace-annotate \"FTP starts\""
$ns at 2.55 "$ns trace-annotate \"FTP stops\""

$ns run


