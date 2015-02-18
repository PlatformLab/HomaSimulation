# slow start protocol in normal situation
# features : labeling, annotation, nam-graph, and window size monitoring

set ns [new Simulator]

$ns color 1 Red

$ns trace-all [open B5-slow-start.tr w]
$ns namtrace-all [open B5-slow-start.nam w]

### build topology with 6 nodes
proc build_topology { ns } {

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

        $ns at 0.0 "$node_(ss1) label TCP2-Sender"
        $ns at 0.0 "$node_(ss2) label CBR2-Sender"
        $ns at 0.0 "$node_(ss3) label TCP2-Receiver"
        $ns at 0.0 "$node_(ss4) label CBR2-Receiver"

        $ns duplex-link $node_(ss1) $node_(rr1) 0.5Mb 20ms DropTail
        $ns duplex-link $node_(ss2) $node_(rr1) 0.5Mb 20ms DropTail
        $ns duplex-link $node_(rr1) $node_(rr2) 0.5Mb 20ms DropTail
        $ns duplex-link $node_(rr2) $node_(ss3) 0.5Mb 20ms DropTail 
        $ns duplex-link $node_(rr2) $node_(ss4) 0.5Mb 20ms DropTail 

        $ns queue-limit $node_(rr1) $node_(rr2) 100
        $ns queue-limit $node_(rr2) $node_(rr1) 100

        $ns duplex-link-op $node_(ss1) $node_(rr1) orient right-down
        $ns duplex-link-op $node_(ss2) $node_(rr1) orient right-up  
        $ns duplex-link-op $node_(rr1) $node_(rr2) orient right     
        $ns duplex-link-op $node_(rr2) $node_(ss3) orient right-up  
        $ns duplex-link-op $node_(rr2) $node_(ss4) orient right-down

        $ns duplex-link-op $node_(rr1) $node_(rr2) queuePos 0.5
        $ns duplex-link-op $node_(rr2) $node_(rr1) queuePos 0.5

}

build_topology $ns

Agent/TCP set nam_tracevar_ true        

### TCP between ss1 and ss3 (Black)
set tcp2 [$ns create-connection TCP $node_(ss1) TCPSink $node_(ss3) 2]
$tcp2 set maxcwnd_ 8
$tcp2 set fid_ 2

set ftp2 [$tcp2 attach-app FTP]
        
$ns add-agent-trace $tcp2 tcp
$ns monitor-agent-trace $tcp2
$tcp2 tracevar cwnd_

### CBR traffic between ss2 and ss4 (Red)
set cbr2 [$ns create-connection CBR $node_(ss2) Null $node_(ss4) 3]
$cbr2 set packetSize_ 500
$cbr2 set interval_ 0.05
$cbr2 set fid_ 3 

proc finish {} {

        global ns
        $ns flush-trace

        puts "filtering..."
        exec tclsh ../bin/namfilter.tcl B5-slow-start.nam
        puts "running nam..."
        exec nam B5-slow-start.nam &
        exit 0
}

### set operations
$ns at 0.1 "$ftp2 start"
$ns at 1.1 "$ftp2 stop"
$ns at 0.1 "$cbr2 start"
$ns at 1.1 "$cbr2 stop"
$ns at 2.0 "finish"

### add annotations
$ns at 0.0 "$ns trace-annotate \"Normal operation of <Slow Start> with max window size, 8\"" 
$ns at 0.1 "$ns trace-annotate \"FTP starts at 0.1\""
$ns at 0.1 "$ns trace-annotate \"CBR starts at 0.1\""

$ns at 0.11 "$ns trace-annotate \"Initial window size is 1\""
$ns at 0.22 "$ns trace-annotate \"1 ack is coming\""
$ns at 0.26 "$ns trace-annotate \"Increase window size to 2\""
$ns at 0.38 "$ns trace-annotate \"2 acks are coming\""
$ns at 0.42 "$ns trace-annotate \"Increase window size to 4\""
$ns at 0.55 "$ns trace-annotate \"4 acks are coming\""
$ns at 0.59 "$ns trace-annotate \"Increase window size to 8\""
$ns at 0.75 "$ns trace-annotate \"8 acks are coming\""
$ns at 0.77 "$ns trace-annotate \"Keep maximum cwnd size, 8\""

$ns at 1.1 "$ns trace-annotate \"FTP stops at 1.1\""
$ns at 1.1 "$ns trace-annotate \"CBR stops at 1.1\""
        
$ns run

