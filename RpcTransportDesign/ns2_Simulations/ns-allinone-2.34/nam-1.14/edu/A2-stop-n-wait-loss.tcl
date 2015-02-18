# stop and wait mechanism with packet loss
# features : labeling, annotation, nam-graph, and window size monitoring

set ns [new Simulator]

set n0 [$ns node]
set n1 [$ns node]

$ns at 0.0 "$n0 label Sender"
$ns at 0.0 "$n1 label Receiver"

set nf [open A2-stop-n-wait-loss.nam w]
$ns namtrace-all $nf
set f [open A2-stop-n-wait-loss.tr w]
$ns trace-all $f

$ns duplex-link $n0 $n1 0.2Mb 200ms DropTail
$ns duplex-link-op $n0 $n1 orient right
$ns duplex-link-op $n0 $n1 queuePos 0.5
$ns queue-limit $n0 $n1 10

Agent/TCP set nam_tracevar_ true

set tcp [new Agent/TCP]
$tcp set window_ 1
$tcp set maxcwnd_ 1
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
$ns at 1.3 "$ns queue-limit $n0 $n1 0"
$ns at 1.5 "$ns queue-limit $n0 $n1 10"
$ns at 3.0 "$ns detach-agent $n0 $tcp ; $ns detach-agent $n1 $sink" 
$ns at 3.5 "finish"

$ns at 0.0 "$ns trace-annotate \"Stop and Wait with Packet Loss\""

$ns at 0.05 "$ns trace-annotate \"FTP starts at 0.1\""

$ns at 0.11 "$ns trace-annotate \"Send Packet_0\""
$ns at 0.35 "$ns trace-annotate \"Receive Ack_0\""
$ns at 0.56 "$ns trace-annotate \"Send Packet_1\""
$ns at 0.79 "$ns trace-annotate \"Receive Ack_1\""
$ns at 0.99 "$ns trace-annotate \"Send Packet_2\""
$ns at 1.23 "$ns trace-annotate \"Receive Ack_2  \""

$ns at 1.43 "$ns trace-annotate \"Lost Packet_3\""
$ns at 1.5 "$ns trace-annotate \"Waiting for Ack_3\""

$ns at 2.43 "$ns trace-annotate \"Send Packet_3 again (cause of timeout)\""
$ns at 2.67 "$ns trace-annotate \"Receive Ack_3\""
$ns at 2.88 "$ns trace-annotate \"Send Packet_4\""

$ns at 3.1 "$ns trace-annotate \"FTP stops\""




proc finish {} {
	global ns nf
	$ns flush-trace
	close $nf

	puts "filtering..."
	exec tclsh ../bin/namfilter.tcl A2-stop-n-wait-loss.nam
        puts "running nam..."
        exec nam A2-stop-n-wait-loss.nam &
	exit 0
}

$ns run
