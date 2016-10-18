source "tcp-common-opt.tcl"

#add/remove packet headers as required
#this must be done before create simulator, i.e., [new Simulator]
#remove-all-packet-headers       ;# removes all except common
#add-packet-header Flags IP RCP  ;#hdrs reqd for RCP traffic

set ns [new Simulator]
puts "Date: [clock format [clock seconds]]"
set sim_start [clock seconds]
puts "Host: [exec uname -a]"

if {$argc != 27} {
    puts "wrong number of arguments $argc"
    exit 0
}

set sim_end [lindex $argv 0]
set link_rate [lindex $argv 1]
set mean_link_delay [lindex $argv 2]
set host_delay [lindex $argv 3]
set queueSize [lindex $argv 4]
set load [lindex $argv 5]
set connections_per_pair [lindex $argv 6]
set meanFlowSize [lindex $argv 7]
#### Multipath
set enableMultiPath [lindex $argv 8]
set perflowMP [lindex $argv 9]
#### Transport settings options
set sourceAlg [lindex $argv 10] ; # Sack or DCTCP-Sack
set ackRatio [lindex $argv 11]
set enableHRTimer 0
set slowstartrestart [lindex $argv 12]
set DCTCP_g [lindex $argv 13] ; # DCTCP alpha estimation gain
set min_rto [lindex $argv 14]
#### Switch side options
set switchAlg [lindex $argv 15]
set DCTCP_K [lindex $argv 16]
set drop_prio_ [lindex $argv 17]
set prio_scheme_ [lindex $argv 18]
set deque_prio_ [lindex $argv 19]
set prob_cap_ [lindex $argv 20]
set keep_order_ [lindex $argv 21]
#### topology
set topology_spt [lindex $argv 22]
set topology_tors [lindex $argv 23]
set topology_spines [lindex $argv 24]
set topology_x [lindex $argv 25]
#### NAM
set enableNAM [lindex $argv 26]

#### Packet size is in bytes.
set pktSize 1460
#### trace frequency
#set queueSamplingInterval 0.0001
set queueSamplingInterval 1

puts "Simulation input:" 
puts "Dynamic Flow - Web Search (DCTCP)"
puts "topology: spines server per rack = $topology_spt, x = $topology_x"
puts "sim_end $sim_end"
puts "link_rate $link_rate Gbps"
puts "link_delay $mean_link_delay sec"
puts "RTT  [expr $mean_link_delay * 2.0 * 6] sec"
puts "host_delay $host_delay sec"
puts "queue size $queueSize pkts"
puts "load $load"
puts "connections_per_pair $connections_per_pair"
puts "enableMultiPath=$enableMultiPath, perflowMP=$perflowMP"
puts "source algorithm: $sourceAlg"
puts "ackRatio $ackRatio"
puts "DCTCP_g $DCTCP_g"
puts "HR-Timer $enableHRTimer"
puts "slow-start Restart $slowstartrestart"
puts "switch algorithm $switchAlg"
puts "DCTCP_K_ $DCTCP_K"
puts "pktSize(payload) $pktSize Bytes"
puts "pktSize(include header) [expr $pktSize + 40] Bytes"

puts "enableNAM $enableNAM"
puts " "

################# Transport Options ####################

Agent/TCP set ecn_ 1
Agent/TCP set old_ecn_ 1
Agent/TCP set packetSize_ $pktSize
Agent/TCP/FullTcp set segsize_ $pktSize
Agent/TCP/FullTcp set spa_thresh_ 0
Agent/TCP set window_ 64
Agent/TCP set windowInit_ 2
Agent/TCP set slow_start_restart_ $slowstartrestart
Agent/TCP set windowOption_ 0
Agent/TCP set tcpTick_ 0.000001
Agent/TCP set minrto_ $min_rto
Agent/TCP set maxrto_ 2

Agent/TCP/FullTcp set nodelay_ true; # disable Nagle
Agent/TCP/FullTcp set segsperack_ $ackRatio; 
Agent/TCP/FullTcp set interval_ 0.000006
if {$perflowMP == 0} {  
    Agent/TCP/FullTcp set dynamic_dupack_ 0.75
}
if {$ackRatio > 2} {
    Agent/TCP/FullTcp set spa_thresh_ [expr ($ackRatio - 1) * $pktSize]
}
if {$enableHRTimer != 0} {
    Agent/TCP set minrto_ 0.00100 ; # 1ms
    Agent/TCP set tcpTick_ 0.000001
}
if {[string compare $sourceAlg "DCTCP-Sack"] == 0} {
    Agent/TCP set ecnhat_ true
    Agent/TCPSink set ecnhat_ true
    Agent/TCP set ecnhat_g_ $DCTCP_g;
}
#Shuang
#Agent/TCP/FullTcp set prio_scheme_ $prio_scheme_;
#Agent/TCP/FullTcp set dynamic_dupack_ 1000000; #disable dupack
Agent/TCP set window_ 1000000
Agent/TCP set windowInit_ 12
#Agent/TCP/FullTcp/Sack set clear_on_timeout_ false;
#Agent/TCP/FullTcp set pipectrl_ true;
#Agent/TCP/FullTcp/Sack set sack_rtx_threshmode_ 2;
Agent/TCP set maxcwnd_ 149
#Agent/TCP/FullTcp set prob_cap_ $prob_cap_;
Agent/TCP set rtxcur_init_ $min_rto;
set myAgent "Agent/TCP/FullTcp/Sack";

################# Switch Options ######################

Queue set limit_ $queueSize

Queue/DropTail set queue_in_bytes_ true
Queue/DropTail set mean_pktsize_ [expr $pktSize+40]
Queue/DropTail set drop_prio_ $drop_prio_
Queue/DropTail set deque_prio_ $deque_prio_
Queue/DropTail set keep_order_ $keep_order_

Queue/RED set bytes_ false
Queue/RED set queue_in_bytes_ true
Queue/RED set mean_pktsize_ $pktSize
Queue/RED set setbit_ true
Queue/RED set gentle_ false
Queue/RED set q_weight_ 1.0
Queue/RED set mark_p_ 1.0
Queue/RED set thresh_ $DCTCP_K
Queue/RED set maxthresh_ $DCTCP_K
Queue/RED set drop_prio_ $drop_prio_
Queue/RED set deque_prio_ $deque_prio_
			 
#DelayLink set avoidReordering_ true

############### NAM ###########################
if {$enableNAM != 0} {
    set namfile [open out.nam w]
    $ns namtrace-all $namfile
}

############## Multipathing ###########################

if {$enableMultiPath == 1} {
    $ns rtproto DV
    Agent/rtProto/DV set advertInterval	[expr 2*$sim_end]  
    Node set multiPath_ 1 
    if {$perflowMP != 0} {
	Classifier/MultiPath set perflow_ 1
    }
}

############# Topology #########################
$ns color 0 Red
$ns color 1 Orange
$ns color 2 Yellow
$ns color 3 Green
$ns color 4 Blue
$ns color 5 Violet
$ns color 6 Brown
$ns color 7 Black

set S [expr $topology_spt * $topology_tors] ; #number of servers
set UCap [expr $link_rate * $topology_spt / $topology_spines / $topology_x] ; #uplink rate

for {set i 0} {$i < $S} {incr i} {
    set s($i) [$ns node]
}

for {set i 0} {$i < $topology_tors} {incr i} {
    set n($i) [$ns node]
    $n($i) shape box
    $n($i) color green
}

for {set i 0} {$i < $topology_spines} {incr i} {
    set a($i) [$ns node]
    $a($i) color blue
    $a($i) shape box
}

for {set i 0} {$i < $S} {incr i} {
    set j [expr $i/$topology_spt]
    #set tmpr [expr $link_rate * 1.1]
    $ns simplex-link $s($i) $n($j) [set link_rate]Gb [expr $host_delay + $mean_link_delay] $switchAlg
    $ns simplex-link $n($j) $s($i) [set link_rate]Gb [expr $host_delay + $mean_link_delay] $switchAlg
    #if {$i == 0} {
    #    set flowmon [$ns makeflowmon Fid]
    #    $ns attach-fmon [$ns link $n($j) $s($i)] $flowmon
    #}

    #$ns queue-limit $s($i) $n($j) 10000

    $ns duplex-link-op $s($i) $n($j) queuePos -0.5    
    set qfile(s$i,n$j) [$ns monitor-queue $s($i) $n($j) [open queue_s$i\_n$j.tr w] $queueSamplingInterval]
    set qfile(n$j,s$i) [$ns monitor-queue $n($j) $s($i) [open queue_n$j\_s$i.tr w] $queueSamplingInterval]

}

for {set i 0} {$i < $topology_tors} {incr i} {
    for {set j 0} {$j < $topology_spines} {incr j} {
	$ns duplex-link $n($i) $a($j) [set UCap]Gb $mean_link_delay $switchAlg	
	$ns duplex-link-op $n($i) $a($j) queuePos 0.25
    }
}

#############  Agents          #########################
set lambda [expr ($link_rate*$load*1000000000)/($meanFlowSize*8.0/1460*1500)]
puts "Arrival: Poisson with inter-arrival [expr 1/$lambda * 1000] ms"
puts "FlowSize: Web Search (DCTCP) with mean = $meanFlowSize"

puts "Setting up connections ..."; flush stdout

set flow_gen 0
set flow_fin 0

set flowlog [open flow.tr w]
set init_fid 0
set tbf 0
for {set j 0} {$j < $S } {incr j} {
    for {set i 0} {$i < $S } {incr i} {
	if {$i != $j} {
	    set agtagr($i,$j) [new Agent_Aggr_pair]
	    $agtagr($i,$j) setup $s($i) $s($j) [array get tbf] 0 "$i $j" $connections_per_pair $init_fid "TCP_pair"
	    $agtagr($i,$j) attach-logfile $flowlog

	    puts -nonewline "($i,$j) "

	    $agtagr($i,$j) set_PCarrival_process  [expr $lambda/($S - 1)] "CDF_search.tcl" [expr 17*$i+1244*$j] [expr 33*$i+4369*$j]
   
	    $ns at 0.1 "$agtagr($i,$j) warmup 0.5 5"
	    $ns at 1 "$agtagr($i,$j) init_schedule"
	    
	    set init_fid [expr $init_fid + $connections_per_pair];
	}
    }
}

puts "Initial agent creation done";flush stdout
puts "Simulation started!"

$ns run
