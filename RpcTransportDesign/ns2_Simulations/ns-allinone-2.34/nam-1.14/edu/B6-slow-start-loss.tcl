# slow start protocol in congestion
# features : labeling, annotation, nam-graph, and window size monitoring
        
set ns [new Simulator]

$ns color 1 Red

$ns trace-all [open B6-slow-start-loss.tr w]
$ns namtrace-all [open B6-slow-start-loss.nam w]

### build topology with 6 nodes
proc build_topology1 { ns } {

	global node_

        set node_(ss1) [$ns node]
        set node_(ss2) [$ns node]
        set node_(rr1) [$ns node]
        set node_(rr2) [$ns node]
        set node_(ss3) [$ns node]
        set node_(ss4) [$ns node]

        $node_(ss2) color "red"
        $node_(ss4) color "red"

        $node_(rr1) color "blue"
        $node_(rr2) color "blue"
        $node_(rr1) shape "rectangular"
        $node_(rr2) shape "rectangular"

        $ns at 0.0 "$node_(ss1) label TCP-Sender"
        $ns at 0.0 "$node_(ss2) label CBR-Sender"
        $ns at 0.0 "$node_(ss3) label TCP-Receiver"
        $ns at 0.0 "$node_(ss4) label CBR-Receiver"

        $ns duplex-link $node_(ss1) $node_(rr1) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(ss2) $node_(rr1) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(rr1) $node_(rr2) 0.5Mb 50ms DropTail
        $ns duplex-link $node_(rr2) $node_(ss3) 0.5Mb 50ms DropTail 
        $ns duplex-link $node_(rr2) $node_(ss4) 0.5Mb 50ms DropTail 

        $ns queue-limit $node_(rr1) $node_(rr2) 10
        $ns queue-limit $node_(rr2) $node_(rr1) 10

        $ns duplex-link-op $node_(ss1) $node_(rr1) orient right-down
        $ns duplex-link-op $node_(ss2) $node_(rr1) orient right-up  
        $ns duplex-link-op $node_(rr1) $node_(rr2) orient right     
        $ns duplex-link-op $node_(rr2) $node_(ss3) orient right-up  
        $ns duplex-link-op $node_(rr2) $node_(ss4) orient right-down

        $ns duplex-link-op $node_(rr1) $node_(rr2) queuePos 0.5
        $ns duplex-link-op $node_(rr2) $node_(rr1) queuePos 0.5

}

build_topology1 $ns

Agent/TCP set nam_tracevar_ true

### TCP between ss1 and ss3 (Black)
set tcp2 [$ns create-connection TCP/Sack1 $node_(ss1) TCPSink/Sack1 $node_(ss3) 2]
$tcp2 set fid_ 0

set ftp2 [$tcp2 attach-app FTP]

$ns add-agent-trace $tcp2 tcp
$ns monitor-agent-trace $tcp2
$tcp2 tracevar cwnd_
        
### CBR traffic between ss2 and ss4 (Red)
set cbr2 [$ns create-connection CBR $node_(ss2) Null $node_(ss4) 3]
$cbr2 set fid_ 1 

proc finish {} {

        global ns
        $ns flush-trace

        puts "filtering..."
        exec tclsh ../bin/namfilter.tcl B6-slow-start-loss.nam
        puts "running nam..."
        exec nam B6-slow-start-loss.nam &
        exit 0
}

### set operations
$ns at 0.1 "$ftp2 start"
$ns at 2.35 "$ftp2 stop"
$ns at 0.1 "$cbr2 start"
$ns at 2.35 "$cbr2 stop"
$ns at 3.0 "finish"

### add annotations
$ns at 0.0 "$ns trace-annotate \"Slow Start with Selective Repeat\""
$ns at 0.1 "$ns trace-annotate \"FTP starts at 0.1\""
$ns at 0.1 "$ns trace-annotate \"CBR starts at 0.1\""

$ns at 0.11 "$ns trace-annotate \"Send Packet_0 	: Initial window size is 1\""
$ns at 0.21 "$ns trace-annotate \"Ack_0			: 1 ack is coming\""
$ns at 0.27 "$ns trace-annotate \"Send Packet_1,2 	: Increase window size to 2\""
$ns at 0.38 "$ns trace-annotate \"Ack_1,2		: 2 acks are coming\""
$ns at 0.45 "$ns trace-annotate \"Send Packet_3,4,5,6	: Increase window size to 4\""
$ns at 0.52 "$ns trace-annotate \"Packet_5,6 are lost\""
$ns at 0.58 "$ns trace-annotate \"Ack_3,4		: 2 acks are coming\""
$ns at 0.63 "$ns trace-annotate \"Send Packet_7,8,9,10	: Keep window size to 4\""
$ns at 0.70 "$ns trace-annotate \"Packet_9 is lost\""
$ns at 0.76 "$ns trace-annotate \"3 Ack_4s		: 3 acks are coming\""

$ns at 0.87 "$ns trace-annotate \"TIMEOUT for Packet_5\""
$ns at 0.89 "$ns trace-annotate \"Send Packet_5\""
$ns at 1.02 "$ns trace-annotate \"Ack_5               	: 1 ack is coming\""
$ns at 1.08 "$ns trace-annotate \"Send Packet_6,9\""
$ns at 1.20 "$ns trace-annotate \"Ack_8,10              : 2 acks are coming\""

$ns at 1.26 "$ns trace-annotate \"Send Packet_11,12,13	: Decrease window size to 3\""
$ns at 1.33 "$ns trace-annotate \"Packet_13 is lost\""
$ns at 1.39 "$ns trace-annotate \"Ack_11,12             : 2 acks are coming\""
$ns at 1.46 "$ns trace-annotate \"Send Packet_14,15	: 1 packet per ack\""
$ns at 1.58 "$ns trace-annotate \"2 Ack_12s             : 2 acks are coming\""

$ns at 1.63 "$ns trace-annotate \"Waiting for Ack_13\""

$ns at 1.96 "$ns trace-annotate \"TIMEOUT,Send Packet_13 : Initialize window size to 1\""
$ns at 2.09 "$ns trace-annotate \"Ack_15                 : 1 ack is coming\""
$ns at 2.15 "$ns trace-annotate \"Send Packet_16,17	 : Increase window size to 2\""
$ns at 2.27 "$ns trace-annotate \"Ack_16,17              : 2 acks are coming\""
$ns at 2.33 "$ns trace-annotate \"Send Packet_18\""
$ns at 2.45 "$ns trace-annotate \"Ack_18\""

$ns at 2.50 "$ns trace-annotate \"FTP stops\"" 
        
$ns run


