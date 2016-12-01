#!/usr/bin/python

"""
This script reads pfabric simulation output file, flow.tr, and calculates
stretch statistics for different message size ranges from the information
provided in the flow.tr.
"""

import sys
import signal
import os
import re
from heapq import *
from numpy import *
from glob import glob
from optparse import OptionParser
from pprint import pprint
import bisect
import pdb

spt = 16 # servers per tor
tors = 9 # num tors per pod
aggrs = 4 # num aggregation switches per pod
pods = 1 # num pods
nicLinkSpeed = 10 # Gbps
fabricLinkSpeed = 40 # Gbps
switchFixDelay = 2.5e-7 # seconds 
edgeLinkDelay = 0 # seconds
fabricLinkDelay = 0 # seconds
hostSwTurnAroundTime = 5e-7 # seconds
hostNicSxThinkTime = 5e-7 # seconds
isFabricCutThrough = False
workload = "FABRICATED_HEAVY_MIDDLE"
loadFactor = 0.80
unschedBytes = 10000
ranges = [51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69,\
    71, 72, 73, 75, 76, 77, 79, 81, 82, 84, 86, 88, 90, 92, 94, 96, 99, 101, 104,\
    107, 110, 113, 116, 120, 124, 128, 132, 136, 141, 147, 152, 158, 165, 172, 180,\
    189, 198, 209, 220, 233, 248, 265, 284, 306, 331, 362, 398, 443, 499, 571, 667,\
    803, 1002, 1061, 1128, 1203, 1290, 1389, 1506, 1644, 1809, 2012, 2266, 2593,\
    3030, 3645, 4572, 6134, 7245, 7607, 8006, 8450, 8946, 9503, 10135, 10857,\
    11689, 12659, 13806, 15180, 16858, 18954, 21645, 25226, 30226, 37700, 50083, 316228]

def serverIdToIp(id):
    numServersPerPod = int(spt*tors)
    podId = int(id)/numServersPerPod
    serverIdInPod = int(id) % numServersPerPod
    torId = serverIdInPod / spt
    serverIdInTor = serverIdInPod % spt
    ipStr = "10.{0}.{1}.{2}".format(podId, torId, serverIdInTor)
    ipInt = 10*(1<<24) + podId*(1<<16) + torId*(1<<8) + serverIdInTor
    return ipInt

def getMinMct(txStart, txPkts, sendrIntIp, recvrIntIp, prevPktArrivals=None):
    """
    For a train of txPkts as [pkt0.byteOnWire, pkt1.byteOnWire,.. ] that start transmission
    at sender at time txStart, this method returns the arrival time of last bit of this
    train at the receiver. prevPktArrivals is the arrival times of
    txPkts train prior this one for this message.
    """
    # Returns the arrival time of last pkt in pkts list when pkts are going
    # across the network through links with speeds in linkSpeeds list
    def pktsDeliveryTime(linkSpeeds, pkts):
        K = len(linkSpeeds) # index of hops starts at zero for sender
                            # host and K for receiver host 
        N = len(pkts) # number of packets

        # arrivals[n][k] gives arrival time of pkts[n-1] at hop k.
        arrivals = [[0]*(K+1) for i in range(N+1)]

        # arrivals[0][:] = 0 (ie. arrival of an imaginary first packet of
        # mesg or last pkt of previous txPkts train) and
        # arrivals[:][0] = txStart (ie. all packets at first hop that is the
        # sender). These are considered for correct boundary conditions of
        # the formula
        if prevPktArrivals:
            arrivals[0][:] = prevPktArrivals

        for i in range(N+1):
            arrivals[i][0] = txStart

        for n in range(1,N+1):
            for k in range(1,K+1):
                arrivals[n][k] = pkts[n-1] * 8.0 * 1e-9 / linkSpeeds[k-1] +\
                    max(arrivals[n-1][k], arrivals[n][k-1])

        return arrivals[-1][:]

    if recvrIntIp == sendrIntIp:
        # When sender and receiver are the same machine
        return txStart

    edgeSwitchFixDelay = switchFixDelay 
    fabricSwitchFixDelay = switchFixDelay
    linkDelays = []
    switchFixDelays = []
    linkSpeeds = []
    totalDelay = 0

    if ((sendrIntIp >> 8) & 255) == ((recvrIntIp >> 8) & 255):
        # receiver and sender are in same rack

        switchFixDelays += [edgeSwitchFixDelay]
        linkDelays += [edgeLinkDelay] * 2

        if not(isFabricCutThrough):
            linkSpeeds = [nicLinkSpeed] * 2
        else:
            # In order to abuse the pktsDeliveryTime function for finding network
            # serializaiton delay when switches are cut through, we need to
            # define linkSpeeds like below
            linkSpeeds = [nicLinkSpeed]

    elif ((sendrIntIp >> 16) & 255) == ((recvrIntIp >> 16) & 255):
        # receiver and sender are in same pod

        switchFixDelays +=\
            [edgeSwitchFixDelay, fabricSwitchFixDelay, edgeSwitchFixDelay]
        linkDelays +=\
            [edgeLinkDelay, fabricLinkDelay,\
            fabricLinkDelay, edgeLinkDelay]

        if not(isFabricCutThrough):
            linkSpeeds =\
                [nicLinkSpeed, fabricLinkSpeed,\
                fabricLinkSpeed, nicLinkSpeed]
        else:
            linkSpeeds = [nicLinkSpeed, fabricLinkSpeed]

    elif ((sendrIntIp >> 24) & 255) == ((recvrIntIp >> 24) & 255):
        # receiver and sender in two different pod

        switchFixDelays +=\
            [edgeSwitchFixDelay, fabricSwitchFixDelay, fabricSwitchFixDelay,\
            fabricSwitchFixDelay, edgeSwitchFixDelay]
        linkDelays +=\
            [edgeLinkDelay, fabricLinkDelay,\
            fabricLinkDelay, fabricLinkDelay,\
            fabricLinkDelay, edgeLinkDelay]

        if not(isFabricCutThrough):
            linkSpeeds =\
                [nicLinkSpeed, fabricLinkSpeed,\
                fabricLinkSpeed, fabricLinkSpeed,\
                fabricLinkSpeed, nicLinkSpeed]
        else:
            linkSpeeds = [nicLinkSpeed, fabricLinkSpeed]
    else:
        raise Exception, 'Sender and receiver IPs dont abide the rules in config.xml file.'

    # Add network serialization delays at switches and sender nic
    pktArrivalsAtHops = pktsDeliveryTime(linkSpeeds, txPkts)
    totalDelay += pktArrivalsAtHops[-1]

    # Add fixed delays:
    totalDelay +=\
        hostSwTurnAroundTime * 2 + hostNicSxThinkTime +\
        sum(linkDelays) + sum(switchFixDelays)

    return totalDelay, pktArrivalsAtHops

if __name__ == '__main__':
    flowFile = os.environ['HOME'] + '/Research/RpcTransportDesign/ns2_Simulations/logs/1121-heavymiddle-pfabric/008-empirical_heavymiddle_pfabric-s16-x1-q13-load0.8-avesize2770.732-mp0-DCTCP-Sack-ar1-SSRtrue-DropTail10000-minrto2.4e-05-droptrue-prio2-dqtrue-prob5-kotrue/flow.tr'
    fd = open(flowFile)
    stretchs = dict()
    N = 0.0
    B = 0.0
    sumBytes = dict()
    for line in fd :
        words = line.split() 
        mesgBytes = int(float(words[0])) # message size in bytes
        mct = float(words[1]) # message completion time
        rttimes = int(words[2]) # retransmission times 
        srcId = int(words[3]) # id of source node
        destId = int(words[4]) # id of destination node

        srcIp = serverIdToIp(srcId) 
        destIp = serverIdToIp(destId)

        mesgPkts = [1500 for i in range(mesgBytes/1460)] + [(mesgBytes%1460) + 40]
        minMct, pktArrivalsAtHops = getMinMct(0, mesgPkts, srcIp, destIp)
        stretch = mct/minMct
        msgrangeInd = bisect.bisect_left(ranges, mesgBytes)
        msgrange = 'inf' if msgrangeInd==len(ranges) else ranges[msgrangeInd]

        N += 1
        B += sum(mesgPkts)
        if msgrange not in stretchs:
            stretchs[msgrange] = []
            sumBytes[msgrange] = 0.0
        bisect.insort(stretchs[msgrange], stretch)
        sumBytes[msgrange] += sum(mesgPkts)
        #print(str(N) + "\n")
    fd.close()

    resultFd = open("StretchVsTransport_{0}_{1}.txt".format(workload, loadFactor), 'w')
    for msgrange,stretch in sorted(stretchs.iteritems()):
        n = len(stretch)
        medianInd = int(floor(n*0.5))
        ninety9Ind = int(floor(n*0.99))
        sizePerc = n / N * 100
        bytesPerc = sumBytes[msgrange] / B * 100
        recordLine =\
            '{0}\t\t{1}\t\t{2}\t\t{3}\t\t{4}\t\t{5}\t\t{6}\t\t{7}\t\t{8}\t\t{9}\n'.format(
            'PseudoIdeal', loadFactor, workload, msgrange, sizePerc, bytesPerc,
            unschedBytes, mean(stretch), 'NA', 'NA')
        resultFd.write(recordLine)

        recordLine =\
            '{0}\t\t{1}\t\t{2}\t\t{3}\t\t{4}\t\t{5}\t\t{6}\t\t{7}\t\t{8}\t\t{9}\n'.format(
            'PseudoIdeal', loadFactor, workload, msgrange, sizePerc, bytesPerc,
            unschedBytes, 'NA', stretch[medianInd], 'NA')
        resultFd.write(recordLine)


        recordLine =\
            '{0}\t\t{1}\t\t{2}\t\t{3}\t\t{4}\t\t{5}\t\t{6}\t\t{7}\t\t{8}\t\t{9}\n'.format(
            'PseudoIdeal', loadFactor, workload, msgrange, sizePerc, bytesPerc,
            unschedBytes, 'NA', 'NA', stretch[ninety9Ind])
        resultFd.write(recordLine)

    resultFd.close()
