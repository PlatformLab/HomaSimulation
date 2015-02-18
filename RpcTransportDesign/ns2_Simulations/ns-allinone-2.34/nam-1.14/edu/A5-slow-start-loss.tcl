# slow start mechanism with packet loss
# features : labeling, annotation, nam-graph, and window size monitoring

set ns [new Simulator]

set n0 [$ns node]
set n1 [$ns node]

$ns at 0.0 "$n0 label Sender"
$ns at 0.0 "$n1 label Receiver"

set nf [open A5-slow-start-loss.nam w]
$ns namtrace-all $nf
set f [open A5-slow-start-loss.tr w]
$ns trace-all $f

$ns duplex-link $n0 $n1 0.2Mb 200ms DropTail
$ns duplex-link-op $n0 $n1 orient right
$ns queue-limit $n0 $n1 10

Agent/TCP set nam_tracevar_ true

set tcp [new Agent/TCP]
$tcp set maxcwnd_ 8
$tcp set windowOption_ 1
$ns attach-agent $n0 $tcp

set sink [new Agent/TCPSink]
$ns attach-agent $n1 $sink

$ns connect $tcp $sink

set ftp [new Application/FTP]
$ftp attach-agent $tcp

$ns add-agent-trace $tcp tcp
$ns monitor-agent-trace $tcp
$tcp tracevar cwnd_

$ns at 0.1 "$ftp start"

# Packet loss 
$ns at 1.87 "$ns queue-limit $n0 $n1 0"
$ns at 1.91 "$ns queue-limit $n0 $n1 10"

$ns at 3.0 "$ns detach-agent $n0 $tcp ; $ns detach-agent $n1 $sink"
$ns at 3.5 "finish"

$ns at 0.0 "$ns trace-annotate \"Slow Start with maximum window size 8 (in congestion)\""

$ns at 0.05 "$ns trace-annotate \"FTP starts at 0.1\""

$ns at 0.11 "$ns trace-annotate \"Send Packet_0 : Initial window size = 1\""
$ns at 0.34 "$ns trace-annotate \"Receive Ack_0\"" 
$ns at 0.56 "$ns trace-annotate \"Send Packet_1,2 : Increase window size to 2\""
$ns at 0.79 "$ns trace-annotate \"Receive Ack_1,2\""
$ns at 0.99 "$ns trace-annotate \"Send Packet_3,4,5,6 : Increase window size to 4\""
$ns at 1.23 "$ns trace-annotate \"Receive Ack_3,4,5,6  \""
$ns at 1.43 "$ns trace-annotate \"Send Packet_7,8,9,10,11,12,13,14 : Increase window size to 8\""
$ns at 1.67 "$ns trace-annotate \"Receive Ack_7,8,9,10,11,12,13,14\""

$ns at 1.88 "$ns trace-annotate \"Send Packet_15\""
$ns at 1.92 "$ns trace-annotate \"Packet_16 is lost\""
$ns at 1.97 "$ns trace-annotate \"Send Packet_17,18,19,20,21,22 : Keep maximum window size,8\""
$ns at 2.11 "$ns trace-annotate \"Receive Ack_15s\""
$ns at 2.20 "$ns trace-annotate \"Receive 6 more Ack_15s\""
$ns at 2.32 "$ns trace-annotate \"Send Packet_23\ : Send only 1 packet cause of window size\""

$ns at 2.48 "$ns trace-annotate \"Re-send lost Packet_16 : Set window size to 1 (Slow Start Restart)\""
$ns at 2.55 "$ns trace-annotate \"Receive Ack_15\""
$ns at 2.72 "$ns trace-annotate \"Receive Ack_23\""
$ns at 2.91 "$ns trace-annotate \"Send Packet_24,25 : Increase window size to 2\""

$ns at 3.1 "$ns trace-annotate \"FTP stops\""

proc finish {} {
        global ns nf
        $ns flush-trace
        close $nf

        puts "filtering..."
        exec tclsh ../bin/namfilter.tcl A5-slow-start-loss.nam
        puts "running nam..."
        exec nam A5-slow-start-loss.nam &
        exit 0
}

$ns run

