source "tcp-common-opt.tcl"

set ns [new Simulator]
puts "Date: [clock format [clock seconds]]"
set sim_start [clock seconds]

if {$argc != 41} {
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
set paretoShape [lindex $argv 7]
set meanFlowSize_1 [lindex $argv 8]
set flow_cdf_1 [lindex $argv 9]
set meanFlowSize_2 [lindex $argv 10]
set flow_cdf_2 [lindex $argv 11]

#### Multipath
set enableMultiPath [lindex $argv 12]
set perflowMP [lindex $argv 13]

#### Transport settings options
set sourceAlg [lindex $argv 14] ; # Sack or DCTCP-Sack
set initWindow [lindex $argv 15]
set ackRatio [lindex $argv 16]
set slowstartrestart [lindex $argv 17]
set DCTCP_g [lindex $argv 18] ; # DCTCP alpha estimation gain
set min_rto [lindex $argv 19]
set prob_cap_ [lindex $argv 20] ; # Threshold of consecutive timeouts to trigger probe mode

#### Switch side options
set switchAlg [lindex $argv 21] ; # DropTail (pFabric), RED (DCTCP) or Priority (PIAS)
set DCTCP_K [lindex $argv 22]
set drop_prio_ [lindex $argv 23]
set prio_scheme_ [lindex $argv 24]
set deque_prio_ [lindex $argv 25]
set keep_order_ [lindex $argv 26]
set prio_num_ [lindex $argv 27]
set ECN_scheme_ [lindex $argv 28]
set pias_thresh_0 [lindex $argv 29]
set pias_thresh_1 [lindex $argv 30]
set pias_thresh_2 [lindex $argv 31]
set pias_thresh_3 [lindex $argv 32]
set pias_thresh_4 [lindex $argv 33]
set pias_thresh_5 [lindex $argv 34]
set pias_thresh_6 [lindex $argv 35]

#### topology
set topology_spt [lindex $argv 36]
set topology_tors [lindex $argv 37]
set topology_spines [lindex $argv 38]
set topology_x [lindex $argv 39]

### result file
set flowlog [open [lindex $argv 40] w]

#### Packet size is in bytes.
set pktSize 1460
#### trace frequency
set queueSamplingInterval 0.0001
#set queueSamplingInterval 1

puts "Simulation input:"
puts "Dynamic Flow - Pareto"
puts "topology: spines server per rack = $topology_spt, x = $topology_x"
puts "sim_end $sim_end"
puts "link_rate $link_rate Gbps"
puts "link_delay $mean_link_delay sec"
puts "host_delay $host_delay sec"
puts "queue size $queueSize pkts"
puts "load $load"
puts "connections_per_pair $connections_per_pair"
puts "enableMultiPath=$enableMultiPath, perflowMP=$perflowMP"
puts "source algorithm: $sourceAlg"
puts "TCP initial window: $initWindow"
puts "ackRatio $ackRatio"
puts "DCTCP_g $DCTCP_g"
puts "slow-start Restart $slowstartrestart"
puts "switch algorithm $switchAlg"
puts "DCTCP_K_ $DCTCP_K"
puts "pktSize(payload) $pktSize Bytes"
puts "pktSize(include header) [expr $pktSize + 40] Bytes"

puts " "

################# Transport Options ####################
Agent/TCP set ecn_ 1
Agent/TCP set old_ecn_ 1
Agent/TCP set packetSize_ $pktSize
Agent/TCP/FullTcp set segsize_ $pktSize
Agent/TCP/FullTcp set spa_thresh_ 0
Agent/TCP set slow_start_restart_ $slowstartrestart
Agent/TCP set windowOption_ 0
Agent/TCP set minrto_ $min_rto
Agent/TCP set tcpTick_ 0.000001
Agent/TCP set maxrto_ 64
Agent/TCP set lldct_w_min_ 0.125
Agent/TCP set lldct_w_max_ 2.5
Agent/TCP set lldct_size_min_ 204800
Agent/TCP set lldct_size_max_ 1048576

Agent/TCP/FullTcp set nodelay_ true; # disable Nagle
Agent/TCP/FullTcp set segsperack_ $ackRatio;
Agent/TCP/FullTcp set interval_ 0.000006

if {$ackRatio > 2} {
    Agent/TCP/FullTcp set spa_thresh_ [expr ($ackRatio - 1) * $pktSize]
}

if {[string compare $sourceAlg "DCTCP-Sack"] == 0} {
    Agent/TCP set ecnhat_ true
    Agent/TCPSink set ecnhat_ true
    Agent/TCP set ecnhat_g_ $DCTCP_g
    Agent/TCP set lldct_ false

} elseif {[string compare $sourceAlg "LLDCT-Sack"] == 0} {
    Agent/TCP set ecnhat_ true
    Agent/TCPSink set ecnhat_ true
    Agent/TCP set ecnhat_g_ $DCTCP_g;
    Agent/TCP set lldct_ true
}

#Shuang
Agent/TCP/FullTcp set prio_scheme_ $prio_scheme_;
Agent/TCP/FullTcp set dynamic_dupack_ 1000000; #disable dupack
Agent/TCP set window_ 1000000
Agent/TCP set windowInit_ $initWindow
Agent/TCP set rtxcur_init_ $min_rto;
Agent/TCP/FullTcp/Sack set clear_on_timeout_ false;
Agent/TCP/FullTcp/Sack set sack_rtx_threshmode_ 2;
Agent/TCP/FullTcp set prob_cap_ $prob_cap_;

Agent/TCP/FullTcp set enable_pias_ false
Agent/TCP/FullTcp set pias_prio_num_ 0
Agent/TCP/FullTcp set pias_debug_ false
Agent/TCP/FullTcp set pias_thresh_0 0
Agent/TCP/FullTcp set pias_thresh_1 0
Agent/TCP/FullTcp set pias_thresh_2 0
Agent/TCP/FullTcp set pias_thresh_3 0
Agent/TCP/FullTcp set pias_thresh_4 0
Agent/TCP/FullTcp set pias_thresh_5 0
Agent/TCP/FullTcp set pias_thresh_6 0

#Whether we enable PIAS
if {[string compare $switchAlg "Priority"] == 0 } {
    Agent/TCP/FullTcp set enable_pias_ true
    Agent/TCP/FullTcp set pias_prio_num_ $prio_num_
    Agent/TCP/FullTcp set pias_debug_ false
    Agent/TCP/FullTcp set pias_thresh_0 $pias_thresh_0
    Agent/TCP/FullTcp set pias_thresh_1 $pias_thresh_1
    Agent/TCP/FullTcp set pias_thresh_2 $pias_thresh_2
    Agent/TCP/FullTcp set pias_thresh_3 $pias_thresh_3
    Agent/TCP/FullTcp set pias_thresh_4 $pias_thresh_4
    Agent/TCP/FullTcp set pias_thresh_5 $pias_thresh_5
    Agent/TCP/FullTcp set pias_thresh_6 $pias_thresh_6
}

if {$queueSize > $initWindow } {
    Agent/TCP set maxcwnd_ [expr $queueSize - 1];
} else {
    Agent/TCP set maxcwnd_ $initWindow
}

set myAgent "Agent/TCP/FullTcp/Sack/MinTCP";

################# Switch Options ######################
Queue set limit_ $queueSize

Queue/DropTail set queue_in_bytes_ true
Queue/DropTail set mean_pktsize_ [expr $pktSize+40]
Queue/DropTail set drop_prio_ $drop_prio_
Queue/DropTail set deque_prio_ $deque_prio_
Queue/DropTail set keep_order_ $keep_order_

Queue/RED set bytes_ false
Queue/RED set queue_in_bytes_ true
Queue/RED set mean_pktsize_ [expr $pktSize+40]
Queue/RED set setbit_ true
Queue/RED set gentle_ false
Queue/RED set q_weight_ 1.0
Queue/RED set mark_p_ 1.0
Queue/RED set thresh_ $DCTCP_K
Queue/RED set maxthresh_ $DCTCP_K
Queue/RED set drop_prio_ $drop_prio_
Queue/RED set deque_prio_ $deque_prio_

Queue/Priority set queue_num_ $prio_num_
Queue/Priority set thresh_ $DCTCP_K
Queue/Priority set mean_pktsize_ [expr $pktSize+40]
Queue/Priority set marking_scheme_ $ECN_scheme_

############## Multipathing ###########################
if {$enableMultiPath == 1} {
    $ns rtproto DV
	Agent/rtProto/DV set advertInterval	[expr 2*$sim_end]
	Node set multiPath_ 1
    if {$perflowMP != 0} {
        Classifier/MultiPath set perflow_ 1
        Agent/TCP/FullTcp set dynamic_dupack_ 0; # enable duplicate ACK
    }
}

############# Topoplgy #########################
set S [expr $topology_spt * $topology_tors] ; #number of servers
set UCap [expr $link_rate * $topology_spt / $topology_spines / $topology_x] ; #uplink rate

puts "UCap: $UCap"

for {set i 0} {$i < $S} {incr i} {
    set s($i) [$ns node]
}

for {set i 0} {$i < $topology_tors} {incr i} {
    set n($i) [$ns node]
}

for {set i 0} {$i < $topology_spines} {incr i} {
    set a($i) [$ns node]
}

############ Edge links ##############
for {set i 0} {$i < $S} {incr i} {
    set j [expr $i/$topology_spt]
    $ns duplex-link $s($i) $n($j) [set link_rate]Gb [expr $host_delay + $mean_link_delay] $switchAlg
}

############ Core links ##############
for {set i 0} {$i < $topology_tors} {incr i} {
    for {set j 0} {$j < $topology_spines} {incr j} {
        $ns duplex-link $n($i) $a($j) [set UCap]Gb $mean_link_delay $switchAlg
    }
}

#############  Agents ################
set lambda_1 [expr ($link_rate*$load*1000000000)/($meanFlowSize_1*8.0/1460*1500)]
set lambda_2 [expr ($link_rate*$load*1000000000)/($meanFlowSize_2*8.0/1460*1500)]

puts "Arrival 1: Poisson with inter-arrival [expr 1/$lambda_1 * 1000] ms"
puts "FlowSize 1: Pareto with mean = $meanFlowSize_1, shape = $paretoShape"
puts "Arrival 2: Poisson with inter-arrival [expr 1/$lambda_2 * 1000] ms"
puts "FlowSize2: Pareto with mean = $meanFlowSize_2, shape = $paretoShape"

puts "Setting up connections ..."; flush stdout

set flow_gen 0
set flow_fin 0

set init_fid 0
for {set j 0} {$j < $S } {incr j} {
    for {set i 0} {$i < $S } {incr i} {
        if {$i != $j} {
                set agtagr($i,$j) [new Agent_Aggr_pair]
                $agtagr($i,$j) setup $s($i) $s($j) "$i $j" $connections_per_pair $init_fid "TCP_pair"
                $agtagr($i,$j) attach-logfile $flowlog
				
				#Create heterogeneous traffic pattern
				if {$i < $j} {
					$agtagr($i,$j) set_PCarrival_process [expr $lambda_1/($S - 1)] $flow_cdf_1 [expr 17*$i+1244*$j] [expr 33*$i+4369*$j]
					puts -nonewline "($i,$j) $flow_cdf_1 "
				} else {
					$agtagr($i,$j) set_PCarrival_process [expr $lambda_2/($S - 1)] $flow_cdf_2 [expr 17*$i+1244*$j] [expr 33*$i+4369*$j]
					puts -nonewline "($i,$j) $flow_cdf_2 "
				}

				$ns at 0.1 "$agtagr($i,$j) warmup 0.5 5"
				$ns at 1 "$agtagr($i,$j) init_schedule"

				set init_fid [expr $init_fid + $connections_per_pair];
            }
        }
}

puts "Initial agent creation done";flush stdout
puts "Simulation started!"

$ns run
