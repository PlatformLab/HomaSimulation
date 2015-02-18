# stop and wait protocol in congestion 
# features : labeling, annotation, nam-graph, and window size monitoring

set ns [new Simulator]

$ns color 1 red

$ns trace-all [open B2-stop-n-wait-loss.tr w]
$ns namtrace-all [open B2-stop-n-wait-loss.nam w]

### build topology with 6 nodes
proc build_topology { ns } {

        global node_

        set node_(s1) [$ns node]
        set node_(s2) [$ns node]
        set node_(r1) [$ns node]
        set node_(r2) [$ns node]
        set node_(s3) [$ns node]
        set node_(s4) [$ns node]

        $node_(s2) color "red"
        $node_(s4) color "red"

        $node_(r1) color "blue"
        $node_(r2) color "blue"
        $node_(r1) shape "rectangular"
        $node_(r2) shape "rectangular"

        $ns at 0.0 "$node_(s1) label Stop&Wait"
        $ns at 0.0 "$node_(s2) label CBR"
        $ns at 0.0 "$node_(s3) label Stop&Wait"
        $ns at 0.0 "$node_(s4) label CBR"

        $ns duplex-link $node_(s1) $node_(r1) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(s2) $node_(r1) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(r1) $node_(r2) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(r2) $node_(s3) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(r2) $node_(s4) 0.5Mb 50ms DropTail

        $ns queue-limit $node_(r1) $node_(r2) 2
        $ns queue-limit $node_(r2) $node_(r1) 2

        $ns duplex-link-op $node_(s1) $node_(r1) orient right-down
        $ns duplex-link-op $node_(s2) $node_(r1) orient right-up
        $ns duplex-link-op $node_(r1) $node_(r2) orient right
        $ns duplex-link-op $node_(r2) $node_(s3) orient right-up
        $ns duplex-link-op $node_(r2) $node_(s4) orient right-down

        $ns duplex-link-op $node_(r1) $node_(r2) queuePos 0.5
        $ns duplex-link-op $node_(r2) $node_(r1) queuePos 0.5

}

build_topology $ns

Agent/TCP set nam_tracevar_ true

### stop-n-wait protocol between s1 and s3 (Black)
set tcp [new Agent/TCP]
$tcp set window_ 1
$tcp set maxcwnd_ 1
$ns attach-agent $node_(s1) $tcp

set sink [new Agent/TCPSink]
$ns attach-agent $node_(s3) $sink

$ns connect $tcp $sink

set ftp [new Application/FTP]
$ftp attach-agent $tcp

$ns add-agent-trace $tcp tcp
$ns monitor-agent-trace $tcp
$tcp tracevar cwnd_

### CBR traffic between s2 and s4 (Red)
set cbr [$ns create-connection CBR $node_(s2) Null $node_(s4) 1]
$cbr set fid_ 1

### set operations
$ns at 0.1 "$ftp start"
$ns at 2.35 "$ftp stop"
$ns at 0.1 "$cbr start"
$ns at 2.35 "$cbr stop"
# $ns at 0.48 "$ns queue-limit $node_(r1) $node_(r2) 1"
$ns at 0.52 "$ns queue-limit $node_(r1) $node_(r2) 2"
$ns at 3.0 "finish"

### add annotation
$ns at 0.0 "$ns trace-annotate \"Stop and Wait with packet loss\""
$ns at 0.05 "$ns trace-annotate \"FTP starts at 0.1\""
$ns at 0.11 "$ns trace-annotate \"Send Packet_0\""
$ns at 0.30 "$ns trace-annotate \"Receive Ack_0\""
$ns at 0.45 "$ns trace-annotate \"Send Packet_1\""
$ns at 0.50 "$ns trace-annotate \"Packet_1 is lost\""
$ns at 0.55 "$ns trace-annotate \"Waiting for Ack_1\""
$ns at 1.34 "$ns trace-annotate \"Timout -> Retransmit Packet_1\""
$ns at 1.55 "$ns trace-annotate \"Receive Ack_1\""
$ns at 1.70 "$ns trace-annotate \"Send Packet_2\""
$ns at 1.90 "$ns trace-annotate \"Receive Ack_2  \""
$ns at 2.0 "$ns trace-annotate \"FTP stops\""

proc finish {} {
	global ns 
	$ns flush-trace

	puts "filtering..."
	exec tclsh ../bin/namfilter.tcl B2-stop-n-wait-loss.nam
        puts "running nam..."
        exec nam B2-stop-n-wait-loss.nam &
	exit 0
}

$ns run






