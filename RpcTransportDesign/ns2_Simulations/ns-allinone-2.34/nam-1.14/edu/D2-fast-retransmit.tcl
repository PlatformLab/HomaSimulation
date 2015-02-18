# It will show how TCP adjusts its window size by multiplicative decrease
# features : labeling, annotation, nam-graph, and window size monitoring

#
#	n0 --- n1 -------------- n2 --- n3
#

set ns [new Simulator]

$ns trace-all [open fast-retransmit.tr w]
$ns namtrace-all [open fast-retransmit.nam w]

### build topology with 4 nodes

        foreach i " 0 1 2 3 " {
                set n$i [$ns node]
        }

        $ns at 0.0 "$n0 label TCP"
        $ns at 0.0 "$n3 label TCP"

        $ns duplex-link $n0 $n1 5Mb 20ms DropTail
        $ns duplex-link $n1 $n2 0.5Mb 100ms DropTail
        $ns duplex-link $n2 $n3 5Mb 20ms DropTail

        $ns queue-limit $n1 $n2 5

        $ns duplex-link-op $n0 $n1 orient right
        $ns duplex-link-op $n1 $n2 orient right     
        $ns duplex-link-op $n2 $n3 orient right     

        $ns duplex-link-op $n1 $n2 queuePos 0.5

### set TCP variables

Agent/TCP set nam_tracevar_ true        
Agent/TCP set window_ 20
Agent/TCP set ssthresh_ 20

### TCP between n0 and n3 (Black)

set tcp [new Agent/TCP/Reno]
$ns attach-agent $n0 $tcp
        
set sink [new Agent/TCPSink]
$ns attach-agent $n3 $sink

$ns connect $tcp $sink

set ftp [new Application/FTP]
$ftp attach-agent $tcp

$ns add-agent-trace $tcp tcp
$ns monitor-agent-trace $tcp
$tcp tracevar cwnd_
$tcp tracevar ssthresh_

proc finish {} {

        global ns
        $ns flush-trace

        puts "filtering..."
        exec tclsh ../bin/namfilter.tcl fast-retransmit.nam
        puts "running nam..."
        exec nam fast-retransmit.nam &
        exit 0
}

### set operations

$ns at 0.1 "$ftp start"
$ns at 5.0 "$ftp stop"
$ns at 5.1 "finish"

### add annotations
$ns at 0.0 "$ns trace-annotate \"TCP with multiplicative decrease\"" 

$ns run

