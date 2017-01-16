#!/usr/bin/python
"""
For a given message size distribution and a nominal load factor, the real
load factor observed on the network is often larger than the nominal load
factor due to extra ack/grant packets and overhead bytes from various header
types.
This script computes the adjusted values for average message size and nominal
load factor for the given message size distribution and real load factor that
user wishes to observe on the network.
"""

import sys
import os
from optparse import OptionParser
import random
import pdb
import numpy as np
sys.path.insert(0, os.environ['HOME'] +\
    '/Research/RpcTransportDesign/OMNeT++Simulation/analysis')
import parseResultFiles as prf

prevSize = 1
class ProtoType:
    homa = 1
    pfabric = 2
    phost = 3


def adjustedMesgSize(size, protoType, withGrantsOrAcks, smooth):
    global prevSize
    sizeUpper = size
    if smooth:
        size = (size + prevSize) / 2
        prevSize = sizeUpper

    if (protoType == ProtoType.homa):
        unschedData = 9328
        fullPkt = 1538
        unschedPkt = 1442
        grantPkt = 84 #The grant size is 80 but the min ethernet frame size,
                      #including ipg, preamble, etc. is 84 bytes.
        schedPkt = 1456

        unschedBytes = min(size, unschedData)
        unschedAdjusted = int(unschedBytes/unschedPkt) * 1538 +\
            ((unschedBytes % unschedPkt) + (fullPkt-unschedPkt) if\
            (unschedBytes % unschedPkt) else 0)
        sizeOnWire = unschedAdjusted

        schedBytes = size-unschedBytes
        if (schedBytes > 0):
            schedAdjusted = int(schedBytes/schedPkt) * 1538 +\
                ( (schedBytes % schedPkt) + (fullPkt-schedPkt) if\
                (schedBytes % schedPkt) else 0)
            sizeOnWire += schedAdjusted
            if (withGrantsOrAcks):
                numSchedPkts = int(schedBytes/schedPkt) +\
                    (1 if schedBytes % schedPkt else 0)
                # add the traffic overhead from grant for each sched pkt
                grantBytes = numSchedPkts*grantPkt
                sizeOnWire += grantBytes

        return sizeOnWire

    if (protoType == ProtoType.pfabric):
        fullPkt = 1500
        dataPerPkt = 1460
        ackPkt = 40
        sizeOnWire = int(size/dataPerPkt) * fullPkt +\
            ((size % dataPerPkt) + (fullPkt-dataPerPkt) if\
            (size % dataPerPkt) else 0)
        if (withGrantsOrAcks):
            numDataPkts = int(size/dataPerPkt) +\
                (1 if size % dataPerPkt else 0)
            sizeOnWire += numDataPkts*ackPkt

        return sizeOnWire

    if (protoType == ProtoType.phost):
        capability_init = 7
        mss = 1460
        hdrSize = 40
        sizeInPkt = int(size/mss) + (1 if (size % mss) else 0)
        sizeOnWire = size + sizeInPkt*hdrSize
        sizeOnWire += hdrSize # for rts pkt
        sizeOnWire += hdrSize # for the last ack pkt
        if sizeInPkt > capability_init:
            sizeOnWire += sizeInPkt*hdrSize # for capability tokens
        return sizeOnWire

    raise Exception('Unknown protoType')

def adjustedLoad(load, distFile, withGrantsOrAcks = True, smooth = False):
    fd = open(distFile)
    sizeFactor = 1460 if distFile == "DCTCP_MsgSizeDist.txt" else 1
    avgSize = float(fd.readline()) * sizeFactor

    avgSizeOnWire = prf.AttrDict({'homa':0.0, 'pfabric':0.0, 'phost':0.0})
    prevCdf = 0.0
    for line in fd:
        size, cdf = map(float, line.split())
        size *= sizeFactor
        sizeOnWire = adjustedMesgSize(size, ProtoType.homa, withGrantsOrAcks,
            smooth)
        avgSizeOnWire.homa += sizeOnWire*(cdf-prevCdf)
        sizeOnWire = adjustedMesgSize(size, ProtoType.pfabric, withGrantsOrAcks,
            smooth)
        avgSizeOnWire.pfabric += sizeOnWire*(cdf-prevCdf)
        sizeOnWire = adjustedMesgSize(size, ProtoType.phost, withGrantsOrAcks,
            smooth)
        avgSizeOnWire.phost += sizeOnWire*(cdf-prevCdf)

        prevCdf = cdf

    if (distFile == 'FacebookKeyValueMsgSizeDist.txt'):
        sizes = list(np.logspace(np.log10(size+1), 7, 5000))
        sigma = 214.476
        k = 0.348238
        y0 = prevCdf
        x0 = size
        y = [(1-(1+k*(x-x0)/sigma)**(-1/k))*(1-y0)+y0 for x in sizes]
        for size, cdf in zip(sizes, y):
            sizeOnWire = adjustedMesgSize(size, ProtoType.homa,
                withGrantsOrAcks, smooth)
            avgSizeOnWire.homa += sizeOnWire*(cdf-prevCdf)
            sizeOnWire = adjustedMesgSize(size, ProtoType.pfabric,
                withGrantsOrAcks, smooth)
            avgSizeOnWire.pfabric += sizeOnWire*(cdf-prevCdf)
            sizeOnWire = adjustedMesgSize(size, ProtoType.phost,
                withGrantsOrAcks, smooth)
            avgSizeOnWire.phost += sizeOnWire*(cdf-prevCdf)
            prevCdf = cdf

    fd.close()
    print("****Homa Transport****")
    printStr = "Workload: {0}, avg mesg size: {1}, avg mesg size on wire: {2},"\
        " nominal load: {3}, real load on network: {4}\n".format(distFile,\
        avgSize, avgSizeOnWire.homa, load*avgSize/avgSizeOnWire.homa, load)
    print(printStr)

    print('-'*80+"\n****pFabric Transport****")
    printStr = "Workload: {0}, avg mesg size: {1}, avg mesg size on wire: {2},"\
        " nominal load: {3}, real load on network: {4}\n".format(distFile,\
        avgSize, avgSizeOnWire.pfabric, load*avgSize/avgSizeOnWire.pfabric, load)
    print(printStr)

    print('-'*80+"\n****phost Transport****")
    printStr = "Workload: {0}, avg mesg size: {1}, avg mesg size on wire: {2},"\
        " nominal load: {3}, real load on network: {4}\n".format(distFile,\
        avgSize, avgSizeOnWire.pfabric, load*avgSize/avgSizeOnWire.phost, load)
    print(printStr)



if __name__ == '__main__':
    parser = OptionParser(description='This script computes the adjusted values'
    ' for average message size and nominal load factor for the given message '
    'size distribution and real load factor that user wishes to observe on the '
    'network.')

    parser.add_option('--loadFactor', metavar='FLOAT', default='0.8',
        dest='load', help='')
    parser.add_option('--distFile', metavar='FILE_NAME',
        default='Fabricated_Heavy_Middle.txt', dest='distFile', help='')
    parser.add_option('--noGrantsOrAcks', action="store_false",
        default=True, dest='withGrantsOrAcks', help='')
    parser.add_option('--smooth', action="store_true",
        default=False, dest='smooth', help='smooth cdf by'\
            'interpolating')



    options,args = parser.parse_args()
    load = float(options.load)
    distFile = options.distFile
    withGrantsOrAcks = options.withGrantsOrAcks
    smooth = options.smooth

    adjustedLoad(load, distFile, withGrantsOrAcks, smooth)
