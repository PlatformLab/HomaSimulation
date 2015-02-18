
# begin common code

set ftime_ 1.5

proc finish {} {
        global ns nf ofile
        $ns flush-trace
        close $nf

        puts "filtering..."
        exec tclsh ../bin/namfilter.tcl ${ofile}.nam
	exec rm -f ${ofile}.de
	exec make_dropevents ${ofile}.tr > ${ofile}.de
        puts "running nam..."
        exec nam ${ofile}.nam &
        exit 0
}

set ns [new Simulator]

set f [open ${ofile}.tr w]
$ns trace-all $f
set nf [open ${ofile}.nam w]
$ns namtrace-all $nf

# define color index
$ns color 0 red
$ns color 1 blue
$ns color 2 chocolate
$ns color 3 red
$ns color 4 brown
$ns color 5 tan
$ns color 6 gold
$ns color 7 black

# define nodes
set n0 [$ns node]
$n0 color blue
$n0 shape circle

set n1 [$ns node]
$n1 color blue
$n1 shape circle

set n2 [$ns node]
$n2 color blue
$n2 shape circle

set n3 [$ns node]
$n3 color blue
$n3 shape circle

set router [$ns node]
$router color red
$router shape hexagon

set r2 [$ns node]
$r2 color blue
$r2 shape hexagon

$ns at 0.0 "$n2 label \"                                                  link(${L1rate},${L1del}) typ\""
$ns at 0.0 "$r2 label \"/---(${L2rate},${L2del})                          \""
$ns at 0.0 "$router label \"                 Q(${L2qlim},${L2type})\""

# define topology
$ns duplex-link $n0 $router $L1rate $L1del $L1type
$ns duplex-link $n2 $router $L1rate $L1del $L1type

$ns duplex-link $router $r2 $L2rate $L2del $L2type

$ns duplex-link $r2 $n1 $L1rate $L1del $L1type
$ns duplex-link $r2 $n3 $L1rate $L1del $L1type

# and orientation
$ns duplex-link-op $n0 $router orient right-down
$ns duplex-link-op $r2 $n1 orient right-up
$ns duplex-link-op $n2 $router orient right-up
$ns duplex-link-op $r2 $n3 orient right-down
$ns duplex-link-op $router $r2 orient right

# queue limitations and nam trace info
$ns queue-limit $router $r2 $L2qlim
$ns simplex-link-op $router $r2 queuePos 0.4

Agent/TCP set nam_tracevar_ true

set tcp [new $tcptype]
$ns attach-agent $n0 $tcp

if [info exists sinktype] {
	set sink [new $sinktype]
} else {
	set sink [new Agent/TCPSink]
}

# just turn these on always, to mark cong action pkts
$sink set ecn_ true
$tcp set ecn_ true

if [info exists ecn] {
	puts "using ECN in router"
	[[$ns link $router $r2] queue] set setbit_ true
}

if [info exists blimit] {
	puts "setting DRR limit to $blimit"
	[[$ns link $router $r2] queue] set blimit_ $blimit
}

$ns attach-agent $n1 $sink
$ns connect $tcp $sink

set ftp [new Application/FTP]
$ftp attach-agent $tcp

#$ns add-agent-trace $tcp tcp
#$ns monitor-agent-trace $tcp
#$tcp tracevar cwnd_

set udp [new Agent/UDP]
$udp set fid_ 1
$ns attach-agent $n2 $udp
set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 1000
$cbr set interval_ 0.02
if [info exists cbrrate] {
	$cbr set interval_ $cbrrate
}
$cbr attach-agent $udp
set na [new Agent/Null]
$ns attach-agent $n3 $na
$ns connect $udp $na

$ns at 0.0 "$ns set-animation-rate 2ms"
$ns at $ftpstart "$ftp start"
$ns at $cbrstart "$cbr start"
if [info exists endtime] {
	set ftime_ $endtime
}

$ns at 0.0 "$ns trace-annotate \"$ofile agent starting\""
$ns at $ftpstart "$ns trace-annotate \"FTP starts at $ftpstart\""
$ns at $cbrstart "$ns trace-annotate \"CBR starts at $cbrstart\""

$ns at [expr $ftime_ + .49] "$ns trace-annotate \"FTP stops\""
$ns at [expr $ftime_ + 0.5] "$ns detach-agent $n0 $tcp ; $ns detach-agent $n1 $sink"
$ns at [expr $ftime_ + 0.55] "finish"

source $ofile.de
# end common code

