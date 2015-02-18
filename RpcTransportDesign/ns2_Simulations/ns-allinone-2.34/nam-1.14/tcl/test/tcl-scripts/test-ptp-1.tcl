# simple stop-ane-wait protocol

set ns [new Simulator]

set n0 [$ns node]
set n1 [$ns node]

$ns at 0.0 "$n0 label Sender"
$ns at 0.0 "$n1 label Receiver"

set nf [open test-ptp-1.nam w]
$ns namtrace-all $nf

$ns duplex-link $n0 $n1 0.2Mb 200ms DropTail
$ns duplex-link-op $n0 $n1 orient right

set tcp [new Agent/TCP]
$tcp set maxcwnd_ 1
$ns attach-agent $n0 $tcp

set sink [new Agent/TCPSink]
$ns attach-agent $n1 $sink
$ns connect $tcp $sink

set ftp [new Application/FTP]
$ftp attach-agent $tcp

### set operations
$ns at 0.01 "$ftp start"
$ns at 2.6 "$ftp stop"
$ns at 2.6 "finish"

### take snapshots
foreach i "0.0 0.3 0.6 0.9 1.2 1.3 1.4 \ 
1.5 1.6 1.7 1.8 1.9 2.0 2.1 2.2 2.3 2.4 2.5" {
$ns at $i "$ns snapshot"
}

### take snapshot operations
$ns at 1.25 "$ns re-rewind-nam"
$ns at 2.2 "$ns rewind-nam"
$ns at 2.55 "$ns terminate-nam"

### add annotations
$ns at 0.0 "$ns trace-annotate \"Simple Stop-and-Wait protocol\""
$ns at 0.01 "$ns trace-annotate \"FTP starts at 0.1\""
$ns at 2.49 "$ns trace-annotate \"FTP stops\""

proc finish {} {
        global ns nf
        $ns flush-trace
        close $nf
        exit 0
}

$ns run

