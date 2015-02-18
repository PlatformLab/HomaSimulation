set opt(tr) out
set opt(namtr)	"test-lan-1.nam"
set opt(seed)	0
set opt(stop)	3
set opt(node)	8

set opt(qsize)	10
set opt(bw)	    1Mb
set opt(delay)	100ms
set opt(ll)	    LL
set opt(ifq)	  Queue/DropTail
set opt(mac)	  Mac/802_3
set opt(chan)	  Channel
set opt(tcp)	  TCP/Reno
set opt(sink)	  TCPSink
set opt(app)	  FTP

proc finish {} {
	global env nshome pwd
	global ns opt
	$ns flush-trace
	exit 0
}

proc create-trace {} {
	global ns opt

  if [file exists $opt(tr)] {
    catch "exec rm -f $opt(tr) $opt(tr)-bw [glob $opt(tr).*]"
  }

  set trfd [open $opt(tr) w]
  $ns trace-all $trfd
	if {$opt(namtr) != ""} {
		$ns namtrace-all [open $opt(namtr) w]
	}
  return $trfd
}

proc create-topology {} {
	global ns opt
	global lan node source node0

	set num $opt(node)
	for {set i 0} {$i < $num} {incr i} {
		set node($i) [$ns node]
		lappend nodelist $node($i)
	}

	set lan [$ns newLan $nodelist $opt(bw) $opt(delay) \
			-llType $opt(ll) -ifqType $opt(ifq) \
			-macType $opt(mac) -chanType $opt(chan)]

	set node0 [$ns node]
	$ns duplex-link $node0 $node(0) 1Mb 100ms DropTail

	$ns duplex-link-op $node0 $node(0) orient right

}

## MAIN ##
set ns [new Simulator]
set trfd [create-trace]

create-topology

set tcp0 [$ns create-connection TCP/Reno $node0 TCPSink $node(7) 0]
$tcp0 set window_ 15

set ftp0 [$tcp0 attach-app FTP]

### set operations
$ns at 0.0 "$ftp0 start"
$ns at $opt(stop) "finish"

### take snapshots
foreach i "0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 \
1.2 1.5 1.8 1.9 2.0 2.1 2.2 2.3 2.4 2.5 2.6 2.7 2.8" {
		$ns at $i "$ns snapshot"
}

### take snapshot operations
$ns at 2.9 "$ns terminate-nam"

$ns run
