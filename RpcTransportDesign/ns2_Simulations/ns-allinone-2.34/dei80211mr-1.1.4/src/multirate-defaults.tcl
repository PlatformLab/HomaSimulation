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

# add both newly-defined headers and other headers we use
# so that remove-all-packet-headers can be called safely

PacketHeaderManager set tab_(PacketHeader/Multirate) 1
PacketHeaderManager set tab_(PacketHeader/Diffusion) 1
PacketHeaderManager set tab_(PacketHeader/ARP) 1
PacketHeaderManager set tab_(PacketHeader/LL) 1
PacketHeaderManager set tab_(PacketHeader/IP) 1
PacketHeaderManager set tab_(PacketHeader/SR) 1
PacketHeaderManager set tab_(PacketHeader/AODV) 1
PacketHeaderManager set tab_(PacketHeader/rtProtoDV) 1
PacketHeaderManager set tab_(PacketHeader/rtProtoLS) 1
PacketHeaderManager set tab_(PacketHeader/Src_rt) 1
PacketHeaderManager set tab_(PacketHeader/GAF) 1
PacketHeaderManager set tab_(PacketHeader/TORA) 1
PacketHeaderManager set tab_(PacketHeader/Mac) 1



Node/MobileNode instproc initMultirateWifi { param0 } {
	$self instvar mac_
	set god [God instance]
	$mac_($param0) nodes [$god num_nodes]
	$mac_($param0) basicMode_ Mode1Mb
	$mac_($param0) dataMode_ Mode1Mb
}

Node/MobileNode instproc setDataMode { param0 mode} {
	$self instvar mac_
	$mac_($param0)  dataMode_ $mode
}


Node/MobileNode instproc setBasicMode { param0 mode} {
	$self instvar mac_
	$mac_($param0) basicMode_ $mode
}

Node/MobileNode instproc setPowerProfile { param0 pp} {
	$self instvar mac_ netif_
	$mac_($param0) powerProfile $pp
	$netif_($param0) powerProfile $pp
}

Node/MobileNode instproc setPER { param0 per} {
	$self instvar mac_
	$mac_($param0) per $per
}


PER set debug_ 0

# default noise for 22 MHz channel at 300K = 9.108e-14
PER set noise_ [ expr {1.38E-23 * 300 * 22E6} ]

PER instproc loadDefaultPERTable {} {
    $self loadPERTable80211gTrivellato
}


PowerProfile set debug_ 0
Mac/802_11/Multirate set useShortPreamble_ false
Mac/802_11/Multirate set gSyncInterval_ 0
Mac/802_11/Multirate set bSyncInterval_ 0

Mac/802_11/Multirate set CWMin_         32
Mac/802_11/Multirate set CWMax_         1024

Mac/802_11/Multirate set VerboseCounters_ 0


# This default value is very conservative, so that all nodes are
# considered for interference calculation. This default value
# therefore yields accurate but computationally intensive simulations.
Channel/WirelessChannel/PowerAware set distInterference_    1000000000



PeerStatsDB/Static set debug_ 0
PeerStatsDB set VerboseCounters_ 0



RateAdapter set numPhyModes_      0
RateAdapter set debug_            0

RateAdapter instproc use80211b {} {
    $self set numPhyModes_ 4
    $self phymode Mode1Mb   0
    $self phymode Mode2Mb   1
    $self phymode Mode5_5Mb 2
    $self phymode Mode11Mb  3
    $self setModeAtIndex 0
}

RateAdapter instproc use80211g {} {
    $self set numPhyModes_  7
    $self phymode Mode6Mb   0
    # Ignoring Mode9Mb which perform worse than Mode12Mb under all circumstances
    $self phymode Mode12Mb  1
    $self phymode Mode18Mb  2
    $self phymode Mode24Mb  3
    $self phymode Mode36Mb  4
    $self phymode Mode48Mb  5
    $self phymode Mode54Mb  6
    $self setModeAtIndex 0
}



RateAdapter instproc attach2mac {mac} {

    $mac eventhandler $self
    $self setmac $mac

}


RateAdapter/RBAR instproc use80211g {} {
    # SNR Threshold values obtained by simulation results for a single
    # node at varying distance using 1500 byte packets with
    # PERTable80211gTrivellato, and in basic mode (no RTS/CTS, unlike
    # original RBAR formulation)  
    $self set numPhyModes_  7
    $self phymode Mode6Mb   0 -1000
    # Ignoring Mode9Mb which perform worse than Mode12Mb under all circumstances
    $self phymode Mode12Mb  1 4.1
    $self phymode Mode18Mb  2 9.3
    $self phymode Mode24Mb  3 10.35
    $self phymode Mode36Mb  4 16.0
    $self phymode Mode48Mb  5 18.8
    $self phymode Mode54Mb  6 23.8
    $self setModeAtIndex 0
}




#Class RateAdapter/ARF -superclass RateAdapter

# Default values for Auto Rate Fallback
# as reported in 
# Gavin Holland, Nitin Vaidya, Paramvir Bahl, 
# "A Rate- Adaptive MAC Protocol for Multi-Hop Wireless
# Networks", in Proceedings of MobiCom 2001, July 2001.

RateAdapter/ARF set timeout_                     0.06
RateAdapter/ARF set numSuccToIncrRate_             10
RateAdapter/ARF set numFailToDecrRate_              2
RateAdapter/ARF set numFailToDecrRateAfterTout_     1
RateAdapter/ARF set numFailToDecrRateAfterIncr_     1

RateAdapter/SNR set pktsize_              1500
RateAdapter/SNR set maxper_               0.1778



