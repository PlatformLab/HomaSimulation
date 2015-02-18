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

####################
# Input Parameters #
####################

if {$argc == 0} {

    set numUserPairs 4 
    set XMAX 10
    set PHYDataRate Mode6Mb
    set run 1

} elseif {$argc < 4} {
    puts ""
    puts "USAGE:  ns $argv0  numUserPairs scenarioWidth  PHYDataRate replicationNumber"  
    puts ""
    puts "All parameters are numeric, except PHYDataRate which is a string in the form 'ModeXXMb',"
    puts " with XX being one of the 802.11b/g phy modes (e.g., 'Mode1Mb, 'Mode5_5Mb', 'Mode18Mb', etc.)"
    puts ""
    exit 0
} else {

    set numUserPairs  [lindex $argv 0]
    set XMAX [lindex $argv 1]
    set PHYDataRate [lindex $argv 2]
    set run [lindex $argv 3]

}

set ns [new Simulator]





# sensing threshold in dB above noise power
set sensingTreshdB 5

#####################
# End of Parameters #
#####################



set resultdir "/tmp"
set tracedir "/tmp"
set machine_name [exec uname -n]

set system_type [exec uname -s]
# Libraries have different names in some operating systems
if {[string match "CYGWIN*" "$system_type"] == 1} {
    load ../src/.libs/cygdei80211mr-0.dll
} else {
    load ../src/.libs/libdei80211mr.so
} 




set opt(chan)          Channel/WirelessChannel/PowerAware
set opt(prop)          Propagation/FreeSpace/PowerAware
set opt(netif)         Phy/WirelessPhy/PowerAware
set opt(mac)           Mac/802_11/Multirate
set opt(ifq)           Queue/DropTail/PriQueue
set opt(ll)            LL
set opt(ant)           Antenna/OmniAntenna
set opt(ifqlen)        10
set opt(seed)          0.0
set opt(rp)            AODV
set opt(x)             1000
set opt(y)             1000

set duration           10


set opt(nn)            [expr 2 * $numUserPairs]  

set fstr ${numUserPairs}_${XMAX}_${PHYDataRate}_${duration}s.${machine_name}

set traceFilename	"${tracedir}/trace_${fstr}.tr"
set statsFilename       "${resultdir}/stats_${fstr}.log"



LL set mindelay_		1us
LL set delay_			1us
LL set bandwidth_		0	;# not used





Node/MobileNode instproc getIfq { param0} {
    $self instvar ifq_    
    return $ifq_($param0) 
}

Node/MobileNode instproc getPhy { param0} {
    $self instvar netif_    
    return $netif_($param0) 
}

set noisePower 7e-11
set per [new PER]
$per loadPERTable80211gTrivellato
$per set noise_ $noisePower
set opt(CSThresh) [expr $noisePower *  pow ( 10 , $sensingTreshdB / 10.0 ) ]
set opt(AffectThresh) [expr $noisePower ]
Phy/WirelessPhy set Pt_ 0.01
Phy/WirelessPhy set freq_ 2437e6
Phy/WirelessPhy set L_ 1.0

Mac/802_11 set bSyncInterval_ 20e-6
Mac/802_11 set gSyncInterval_ 10e-6

Mac/802_11 set ShortRetryLimit_ 3
Mac/802_11 set LongRetryLimit_ 5

Mac/802_11/Multirate set RTSThreshold_ 100000

Mac/802_11/Multirate set dump_interf_ 0

$per set debug_ 0
PowerProfile set debug_ 0
Phy/WirelessPhy set debug_ 0
Mac/802_11/Multirate set debug_ 0
Phy/WirelessPhy set CSThresh_ $opt(CSThresh)

set chan_               [new $opt(chan)]
set topo                [new Topography]
set traceFilePtr        [open  $traceFilename w]


set startrng [new RNG]
set positionrng [new RNG]
if {$run > 1 } {
    puts "replication number: $run"
    for {set j 1} {$j < $run} {incr j} {
	$startrng next-substream
	$positionrng next-substream
    }
}

set startinterval [expr $opt(nn) * 0.02]
set minstarttime 1

set rvstart [new RandomVariable/Uniform]
$rvstart set min_ $minstarttime
$rvstart set max_ [expr $minstarttime + $startinterval]
$rvstart use-rng $startrng

set rvposition [new RandomVariable/Uniform]
$rvposition set min_ 0
$rvposition set max_ $XMAX
$rvposition use-rng $positionrng


$ns trace-all $traceFilePtr

$topo load_flatgrid $opt(x) $opt(y)

set god_ [create-god [expr $opt(nn)] ]

$ns node-config -adhocRouting $opt(rp) \
            -llType $opt(ll) \
            -macType $opt(mac) \
            -ifqType $opt(ifq) \
            -ifqLen $opt(ifqlen) \
            -antType $opt(ant) \
            -propType $opt(prop) \
	    -phyType $opt(netif) \
	    -topoInstance $topo \
	    -agentTrace OFF \
            -routerTrace OFF \
	    -macTrace ON \
            -ifqTrace OFF \
	    -channel $chan_





# Creating nodes


for {set i 0} {$i < [expr $opt(nn)]} {incr i} {

    set n($i)  [$ns node]
    
    #   This is replaced below by '$mac_ nodes'
    #	$n($i) initMultirateWifi 0 

    $n($i) setPowerProfile 0 [new PowerProfile]

    set mac($i) [$n($i) getMac 0]
    set ifq($i) [$n($i) getIfq 0]
    set phy($i) [$n($i) getPhy 0]

    $mac($i) dataMode_ $PHYDataRate
    $mac($i) basicMode_ Mode6Mb
    $mac($i) nodes $opt(nn)
    $phy($i) set CSTresh_ $opt(CSThresh) 

    $n($i) setPER 0 $per

    $n($i) random-motion 0

}


for {set i 0} {$i < $opt(nn)} {incr i} {
    $n($i) set X_ [$rvposition value]
    $n($i) set Y_ [$rvposition value]
    $n($i) set Z_ 0

    # output to screen
    set xstr [format "%7.4f" [$n($i) set X_]]
    set ystr [format "%7.4f" [$n($i) set Y_]]
    puts " node $i is at   X = $xstr   Y = $ystr "

}



# configuring agents for all nodes

for {set i 0} {$i < $numUserPairs } {incr i} {

    # node $i is the TCP source
    set tcp($i)     [new Agent/TCP/Reno ]
    $tcp($i) set fid_ $i
    $ns attach-agent $n($i) $tcp($i)
    set ftp($i) [new Application/FTP]
    $ftp($i) attach-agent $tcp($i)

    # node $i + $numUserPairs is the other peer
    set j [expr $numUserPairs + $i ]
    set partner($i) $j
    set partner($j) $i

    set tcpsink($j) [new Agent/TCPSink]
    $ns attach-agent $n($j) $tcpsink($j)

    
    $ns connect $tcp($i) $tcpsink($j)

    set starttime($i) [$rvstart value ]
    set stoptime($i) [expr $starttime($i) + $duration]


    $ns at  $starttime($i)  "$ftp($i) start"
    $ns at  $stoptime($i)  "$ftp($i) stop"
}




proc stats {} {
    global ns n opt statsFilename satagent numUserPairs sensingTreshdB starttime stoptime mac partner run 
    global XMAX duration PHYDataRate tcp 

    puts "printing stats"
    set statsFilePtr  [open  $statsFilename a]

    for {set i 0} {$i < $numUserPairs} {incr i} {

	set thrtcp [ expr [ $tcp($i) set ack_  ] * [$tcp($i) set packetSize_] *8 / [expr  $stoptime($i) - $starttime($i)] ]

	set j $partner($i)

	set outputstr " TCP throughput node $i --> node $j : $thrtcp bps" 

	puts $statsFilePtr ${outputstr}
	puts ${outputstr}


    }
    close  $statsFilePtr
    puts ""
    puts "statsfile: $statsFilename"
    
}



proc finish {} {
    global ns opt traceFilePtr traceFilename
    stats
    $ns flush-trace
    close $traceFilePtr
    for {set i 1} {$i < $opt(nn)} {incr i} {

    }

    puts ""
    puts "Tracefile: $traceFilename"
    puts ""
    exit 0
}


# This is  just to fake wireless channel into considering more nodes
# $ns at 0 "$phy(1) set CSThresh_ $opt(AffectThresh)"
# $ns at 0.0001 "$satagent(1) ping"
# $ns at 0.001 "$phy(1) set CSThresh_ $opt(CSThresh)"


$ns at [expr $duration + 2 + $numUserPairs] "finish"

flush stdout
$ns run
