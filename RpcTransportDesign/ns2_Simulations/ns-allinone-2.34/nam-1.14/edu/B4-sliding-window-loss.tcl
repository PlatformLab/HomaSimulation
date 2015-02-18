# sliding window protocol in congestion
# features : labeling, annotation, nam-graph, and window size monitoring
# all packets look black cause of supporting nam-graph
# You could run 'B4-sliding-color.nam' to see the colors of each traffic

set ns [new Simulator]

$ns color 1 red

#$ns trace-all [open B4-sliding-window-loss.tr w]
#$ns namtrace-all [open B4-sliding-window-loss.nam w]


        foreach i "s1 s2 r1 r2 s3 s4" {
               set node_($i) [$ns node]
        }

        $node_(s2) color "red"
        $node_(s4) color "red"

        $node_(r1) color "blue"
        $node_(r2) color "blue"
	$node_(r1) shape "rectangular"
        $node_(r2) shape "rectangular"

        $ns at 0.0 "$node_(s1) label Sliding-W-sender"
        $ns at 0.0 "$node_(s2) label CBR-sender"
        $ns at 0.0 "$node_(s3) label Sliding-W-receiver"
        $ns at 0.0 "$node_(s4) label CBR-receiver"

	$ns duplex-link $node_(s1) $node_(r1) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(s2) $node_(r1) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(r1) $node_(r2) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(r2) $node_(s3) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(r2) $node_(s4) 0.5Mb 50ms DropTail

	$ns queue-limit $node_(r1) $node_(r2) 10
        $ns queue-limit $node_(r2) $node_(r1) 10

	$ns duplex-link-op $node_(s1) $node_(r1) orient right-down
        $ns duplex-link-op $node_(s2) $node_(r1) orient right-up
        $ns duplex-link-op $node_(r1) $node_(r2) orient right
        $ns duplex-link-op $node_(r2) $node_(s3) orient right-up
        $ns duplex-link-op $node_(r2) $node_(s4) orient right-down

        $ns duplex-link-op $node_(r1) $node_(r2) queuePos 0.5
        $ns duplex-link-op $node_(r2) $node_(r1) queuePos 0.5

Agent/TCP set nam_tracevar_ true
# setting sliding window size to 4
Agent/TCP set maxcwnd_ 4

### sliding-window protocol between s1 and s3 (Black)

#set sliding [new Agent/TCP/SlidingWindow]
set sliding [new Agent/TCP]
$sliding set fid_ 0
$ns attach-agent $node_(s1) $sliding

#set sink [new Agent/TCPSink/SlidingWindowSink]
set sink [new Agent/TCPSink]
$ns attach-agent $node_(s3) $sink

$ns connect $sliding $sink

set ftp [new Application/FTP]
$ftp attach-agent $sliding

$ns add-agent-trace $sliding tcp
$ns monitor-agent-trace $sliding
$sliding tracevar cwnd_

### CBR traffic between s2 and s4 (Red)
set cbr [$ns create-connection CBR $node_(s2) Null $node_(s4) 1]
$cbr set fid_ 1

proc finish {} {

	global ns
	$ns flush-trace

#	puts "filtering..."
#	exec tclsh ../bin/namfilter.tcl B4-sliding-window-loss.nam
	puts "running nam..."
	exec nam B4-sliding-window-loss.nam &
	exit 0
}

### set operations
$ns at 0.1 "$ftp start"
$ns at 2.35 "$ftp stop"
$ns at 0.1 "$cbr start"
$ns at 2.35 "$cbr stop"
$ns at 3.0 "finish"

### add annotations
$ns at 0.0 "$ns trace-annotate \"Sliding Window with packet loss    \"" 

$ns run








