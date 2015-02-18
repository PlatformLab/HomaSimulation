#
# Copyright (c) 2007 Regents of the SIGNET lab, University of Padova.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University of Padova (SIGNET lab) nor the 
#    names of its contributors may be used to endorse or promote products 
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

 
########################################
# Command-line parameters
########################################


if {$argc == 0} {

    # default parameters if none passed on the command line
    set opt(nn) 5 
    set opt(xdist) 100
    set opt(raname) arf
    set opt(run) 1

} elseif {$argc != 4 } {
    puts " invalid argc ($argc)"
    puts " usage: $argv0 numnodes xdist RateAdapter replicationnumber"
    exit
} else {

    set opt(nn)      [lindex $argv 0]       
    set opt(xdist)   [lindex $argv 1]
    set opt(raname)  [lindex $argv 2]
    set opt(run)     [lindex $argv 3]

}


if { $opt(raname) == "snr" } {
    set opt(ra) "SNR"
} elseif { $opt(raname) == "arf" } {
    set opt(ra) "ARF"    
} else {
    puts "unknown rate adaptation scheme \"$opt(raname)\""
    exit
} 






########################################
# Scenario Configuration
########################################


# duration of each transmission
set opt(duration)     50

# starting time of each transmission
set opt(startmin)     1
set opt(startmax)     1.20

set opt(resultdir) "/tmp"
set opt(tracedir) "/tmp"
set machine_name [exec uname -n]
set opt(fstr)        ${argv0}_${opt(nn)}_${opt(raname)}.${machine_name}
set opt(resultfname) "${opt(resultdir)}/stats_${opt(fstr)}.log"
set opt(tracefile)   "${opt(tracedir)}/${argv0}.tr"

########################################
# Module Libraries
########################################


# The following lines must be before loading libraries
# and before instantiating the ns Simulator
#
remove-all-packet-headers



set system_type [exec uname -s]

if {[string match "CYGWIN*" "$system_type"] == 1} {
    load ../src/.libs/cygdei80211mr-0.dll
} else {
#if {string match Linux* $system_type} 
    load ../src/.libs/libdei80211mr.so
} 



########################################
# Simulator instance
########################################


set ns [new Simulator]


########################################
# Random Number Generators
########################################

global defaultRNG
set startrng [new RNG]

set rvstart [new RandomVariable/Uniform]
$rvstart set min_ $opt(startmin)
$rvstart set max_ $opt(startmax)
$rvstart use-rng $startrng

# seed random number generator according to replication number
for {set j 1} {$j < $opt(run)} {incr j} {
    $defaultRNG next-substream
    $startrng next-substream
}




########################################
# Some procedures 
########################################

proc finish {} {
    global ns tf opt 
    puts "done!"
    $ns flush-trace
    close $tf
    print_stats
    $ns halt 
    puts " "
    puts "Tracefile     : $opt(tracefile)"
    puts "Results file  : $opt(resultfname)"
    
    set title "node moving from 0m at [expr int($opt(startmoving))]s to $opt(xdist)m at [expr int($opt(endmoving))]s, [expr $opt(nn) - 1] interferers, $opt(ra) scheme"    
}



proc print_stats  {} {
    global fh_cbr opt me_mac me_phy peerstats 

    set resultsFilePtr [open $opt(resultfname) w]

    #for {set id 1} {$id <= 1 } {incr id} 
    for {set id 1} {$id <= $opt(nn) } {incr id}  {


	set snr [$me_mac($id) getAPSnr ]
	set peerstatsstr [$peerstats getPeerStats $id 0]
	set mestatsstr   [$me_mac($id) getMacCounters]
	set logstr "$opt(run) $opt(nn) $opt(duration) $id $snr  $mestatsstr $peerstatsstr "

	#puts $logstr
 	puts $resultsFilePtr $logstr

    }

    close $resultsFilePtr

}


Node/MobileNode instproc getIfq { param0} {
    $self instvar ifq_    
    return $ifq_($param0) 
}

Node/MobileNode instproc getPhy { param0} {
    $self instvar netif_    
    return $netif_($param0) 
}



########################################
# Override Default Module Configuration
########################################

PeerStatsDB set VerboseCounters_ 1
PeerStatsDB/Static set debug_ 0

Mac/802_11/Multirate set VerboseCounters_ 1


Mac/802_11 set RTSThreshold_ 50000
Mac/802_11 set ShortRetryLimit_ 8
Mac/802_11 set LongRetryLimit_  5
Mac/802_11/Multirate set useShortPreamble_ true
Mac/802_11/Multirate set gSyncInterval_ 0.000005
Mac/802_11/Multirate set bSyncInterval_ 0.00001
Mac/802_11 set CWMin_         32
Mac/802_11 set CWMax_         1024


Phy/WirelessPhy set Pt_ 0.01
Phy/WirelessPhy set freq_ 2437e6
Phy/WirelessPhy set L_ 1.0


Queue/DropTail/PriQueue set Prefer_Routing_Protocols    1
Queue/DropTail/PriQueue set size_ 10

set noisePower 7e-11
Phy/WirelessPhy set CSTresh_ [expr $noisePower * 1.1]

Channel/WirelessChannel/PowerAware set distInterference_ 200



###############################
# Global allocations
###############################



set tf [open $opt(tracefile) w]
$ns trace-all $tf

set channel   [new Channel/WirelessChannel/PowerAware]

set topo      [new Topography]
$topo load_flatgrid [expr $opt(xdist) + 1] [expr $opt(xdist) + 1]

set god       [create-god [expr $opt(nn) + 1]]

set per       [new PER]
$per set noise_ $noisePower
$per loadPERTable80211gTrivellato

set peerstats [new PeerStatsDB/Static]
$peerstats numpeers [expr $opt(nn) + 1]




###############################
# Node Configuration
###############################


$ns node-config -adhocRouting DumbAgent \
            -llType  LL    \
            -macType Mac/802_11/Multirate \
            -ifqType Queue/DropTail/PriQueue \
            -ifqLen 10 \
            -antType Antenna/OmniAntenna \
            -propType Propagation/FreeSpace/PowerAware \
	    -phyType Phy/WirelessPhy/PowerAware \
	    -topoInstance $topo \
	    -agentTrace OFF \
            -routerTrace OFF \
	    -macTrace ON \
            -ifqTrace OFF \
	    -channel $channel



#######################################
#  Create Base Station (Access Point) #
#######################################


set bs_node        [$ns node]

set bs_mac [$bs_node getMac 0]
set bs_ifq [$bs_node getIfq 0]
set bs_phy [$bs_node getPhy 0]

$bs_mac nodes [expr $opt(nn) + 1]
$bs_mac  basicMode_ Mode6Mb
$bs_mac  dataMode_ Mode6Mb
$bs_mac  per $per
$bs_mac  PeerStatsDB $peerstats
set bs_pp  [new PowerProfile]
$bs_mac  powerProfile $bs_pp
$bs_phy   powerProfile $bs_pp

set bs_macaddr [$bs_mac id]
$bs_mac     bss_id $bs_macaddr

$bs_node random-motion 0
$bs_node set X_ 0
$bs_node set Y_ 0
$bs_node set Z_ 0



###############################
#  Create Mobile Equipments 
###############################

for {set id 1} {$id <= $opt(nn)} {incr id} {

    set me_node($id)   [$ns node]

    set me_mac($id) [$me_node($id) getMac 0]
    set me_ifq($id) [$me_node($id) getIfq 0]
    set me_phy($id) [$me_node($id) getPhy 0]
    
    $me_mac($id) nodes [expr $opt(nn) + 1]
    $me_mac($id)   basicMode_ Mode6Mb
    $me_mac($id)   dataMode_ Mode54Mb
    $me_mac($id)   per $per
    $me_mac($id)   PeerStatsDB $peerstats
    set me_pp($id)   [new PowerProfile]
    $me_mac($id)   powerProfile $me_pp($id) 
    $me_phy($id)    powerProfile $me_pp($id) 
    $me_mac($id)  bss_id $bs_macaddr

    $me_node($id) set X_ 0
    $me_node($id) set Y_ 0
    $me_node($id) set Z_ 0
    $me_node($id) random-motion 0


    # Create Protocol Stack 

    set me_tran($id)   [new Agent/UDP]
    $ns attach-agent   $me_node($id) $me_tran($id)
    set me_cbr($id)    [new Application/Traffic/CBR]
    $me_cbr($id)    attach-agent $me_tran($id)
    # For each agent at the ME  we also need a corresponding one at the BS

    set bs_tran($id)   [new Agent/Null]
    $ns attach-agent $bs_node $bs_tran($id)

    $ns connect $me_tran($id) $bs_tran($id) 


    # setup socket connection between ME and BS
    $ns connect $me_tran($id) $bs_tran($id) 


    # schedule data transfer
    set cbr_start($id) [$rvstart value]
    set cbr_stop($id) [expr $cbr_start($id) + $opt(duration)]


    # set traffic type
    if {$id == 1} {

	# Test node (id==1) generates CBR traffic station using PHY rate adaptation


	$me_cbr($id) set packetSize_      1000
	$me_cbr($id) set rate_            1200000
	$me_cbr($id) set random_          0

 	set speed($id) [expr int ( ($opt(xdist) + 0.0) / ($opt(duration)/2.0) )]
 	puts "Speed of test node: $speed($id) m/s"

	set opt(startmoving) [expr $cbr_start($id) + $opt(duration) / 2.0]
	set opt(endmoving)   [expr $cbr_start($id) + $opt(duration) ]

 	$ns at $opt(startmoving) "$me_node($id) setdest $opt(xdist) 0.001 $speed($id)"

	set pktsize [expr [$me_cbr($id) set packetSize_] + 0.0]


	set me_ra($id) [new RateAdapter/$opt(ra)]

	if { $opt(ra) == "SNR" } {
	    $me_ra($id) set pktsize_ $pktsize

	} 

	$me_ra($id) attach2mac $me_mac($id)
	$me_ra($id) use80211g
	$me_ra($id) setmodeatindex 0


    } else {

	# All other nodes (id!=1) are in saturation and use a fixed PHY rate
	# Saturated station

	$me_cbr($id) set packetSize_      1500
	$me_cbr($id) set rate_            [ expr 54000000 / ( $opt(nn) - 1) ]

	# These are needed to match Bianchi Model
	$me_mac($id) set ShortRetryLimit_ 100
	$me_mac($id) set LongRetryLimit_  100

    }

    $ns at $cbr_start($id)  "$me_cbr($id) start"
    $ns at $cbr_stop($id)   "$me_cbr($id) stop"



}




###############################
#  Start Simulation
###############################

puts -nonewline "Simulating"


for {set i 1} {$i < 40} {incr i} {
    $ns at [expr $opt(startmax) + (($opt(duration) * $i)/ 40.0) ] "puts -nonewline . ; flush stdout"
}
$ns at [expr $opt(startmax) + $opt(duration) + 5] "finish"
$ns run
