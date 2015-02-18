# stop and wait protocol in normal situation 
# features : labeling, annotation, nam-graph, and window size monitoring

set ns [new Simulator]

$ns color 1 red

$ns trace-all [open B1-stop-n-wait.tr w]
$ns namtrace-all [open B1-stop-n-wait.nam w]

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

        $ns at 0.0 "$node_(s1) label Sender"
        $ns at 0.0 "$node_(s2) label CBR-sender"
        $ns at 0.0 "$node_(s3) label Receiver"
        $ns at 0.0 "$node_(s4) label CBR-receiver"

        $ns duplex-link $node_(s1) $node_(r1) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(s2) $node_(r1) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(r1) $node_(r2) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(r2) $node_(s3) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(r2) $node_(s4) 0.5Mb 50ms DropTail

        $ns queue-limit $node_(r1) $node_(r2) 100
        $ns queue-limit $node_(r2) $node_(r1) 100

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
$cbr set packetSize_ 500
$cbr set interval_ 0.05
$cbr set class_ 1

### set operations
$ns at 0.1 "$ftp start"
$ns at 1.7 "$ftp stop"
$ns at 0.1 "$cbr start"
$ns at 1.7 "$cbr stop"
$ns at 2.0 "finish"

### add annotation
$ns at 0.0 "$ns trace-annotate \"Stop and Wait with packet loss\""
$ns at 0.05 "$ns trace-annotate \"FTP starts at 0.1\""
$ns at 0.11 "$ns trace-annotate \"Send Packet_0\""
$ns at 0.30 "$ns trace-annotate \"Receive Ack_0\""
$ns at 0.45 "$ns trace-annotate \"Send Packet_1\""
$ns at 0.65 "$ns trace-annotate \"Receive Ack_1\""
$ns at 0.80 "$ns trace-annotate \"Send Packet_2\""
$ns at 1.00 "$ns trace-annotate \"Receive Ack_2\""
$ns at 1.15 "$ns trace-annotate \"Send Packet_3\""
$ns at 1.35 "$ns trace-annotate \"Receive Ack_3\""
$ns at 1.50 "$ns trace-annotate \"Send Packet_4\""
$ns at 1.70 "$ns trace-annotate \"Receive Ack_4\""
$ns at 1.80 "$ns trace-annotate \"FTP stops\""

proc finish {} {
	global ns 
	$ns flush-trace

	puts "filtering..."
	exec tclsh ../bin/namfilter.tcl B1-stop-n-wait.nam
        puts "running nam..."
        exec nam B1-stop-n-wait.nam &
	exit 0
}

$ns run
