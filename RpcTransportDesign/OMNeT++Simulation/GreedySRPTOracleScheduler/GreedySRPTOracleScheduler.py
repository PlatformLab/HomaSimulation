#!/usr/bin/python

"""
This scripts simulates a central scheduler that has global information about
all messages that enter the network at every sender. It uses an SRPT policy to
find a nonconflicting transmission schedule from senders to receivers.
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

sys.path.insert(0, os.environ['HOME'] +
    '/Research/RpcTransportDesign/OMNeT++Simulation/analysis')
sys.setrecursionlimit(10000)
import parseResultFiles as prf

ETHERNET_PREAMBLE_SIZE = 8
ETHERNET_HDR_SIZE = 14
ETHERNET_CRC_SIZE = 4
INTER_PKT_GAP = 12
MIN_ETHERNET_PAYLOAD_BYTES = 46
MAX_ETHERNET_PAYLOAD_BYTES = 1500
MAX_ETHERNET_FRAME_SIZE = MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_PREAMBLE_SIZE +\
    ETHERNET_HDR_SIZE + ETHERNET_CRC_SIZE + INTER_PKT_GAP
IP_HEADER_SIZE = 20
UDP_HEADER_SIZE = 8
HOMA_UNSCHED_HDR = 30

def ipIntToStr(ipInt):
    ipStr =\
        '{0}.{1}.{2}.{3}'.format((ipInt >> 24) & 255, (ipInt >> 16) & 255,\
        (ipInt >> 8) & 255, ipInt & 255)
    return ipStr

def parseSimParams(simParams):
    """
    Input arg is simulation paramter dic parsed out by the parseResultFile
    module.  Returns simulation parameter list such that its usable by the
    components of GreedySRPTOracleScheduler module (ie. component defined in
    this file.)
    """
    paramDic = prf.AttrDict()
    paramDic.nicLinkSpeed = int(simParams.nicLinkSpeed.strip('Gpbs'))
    paramDic.fabricLinkSpeed = int(simParams.fabricLinkSpeed.strip('Gbps'))
    paramDic.numTors = int(simParams.numTors)
    paramDic.numServersPerTor = int(simParams.numServersPerTor)
    paramDic.edgeLinkDelay = float(simParams.edgeLinkDelay.strip('us'))*1e-6
    paramDic.fabricLinkDelay = float(simParams.fabricLinkDelay.strip('us'))*1e-6
    paramDic.hostNicSxThinkTime = float(
        simParams.hostNicSxThinkTime.strip('us'))*1e-6

    paramDic.hostSwTurnAroundTime = float(
        simParams.hostSwTurnAroundTime.strip('us'))*1e-6

    paramDic.isFabricCutThrough = simParams.isFabricCutThrough == 'true'
    paramDic.switchFixDelay = float(simParams.switchFixDelay.strip('us'))*1e-6
    paramDic.msgSizeRanges = [float(strSizeBound) for strSizeBound in\
        simParams.msgSizeRanges.strip('\"').split()]

    paramDic.workloadType = simParams.workloadType
    paramDic.realLoadFactor = 100*float(simParams.rlf)    # network load factor
    paramDic.loadFactor = 100*float(eval(simParams.loadFactor)) # good put at
                                                                # the app
    return paramDic

def preprocInputData(vecFile, vciFile, scaFile):
    """
    Inputs are the file names for vector and scalar result files that contain
    the traffice data from simulation, along with the index file name from
    simulation. The output is parsed traffice data and simulation information
    for this module.
    """
    sp = prf.ScalarParser(scaFile)
    numHosts =\
        int(sp.generalInfo.numTors) * int(sp.generalInfo.numServersPerTor)

    # IP addr of host with id equal to 'i' is at index i of the list. IPs are
    # recorded as tuples of int and string values: (int, 'YYY.YYY.YYY.YYY')
    hostIdIpAddr = list()

    # Dictionary that maps the integer values of host IP address to hostId
    ipHostId = dict()

    for hostId in range(numHosts):
         ipInt = int(sp.hosts.access('host[{0}].trafficGeneratorApp[0].'
            '"Host IPv4 address".value'.format(hostId)))

         ipStr = ipIntToStr(ipInt)
         hostIdIpAddr.append((ipInt, ipStr))
         ipHostId[ipInt] = hostId


    vp = prf.VectorParser(vciFile, vecFile)
    msgSizeVecName = 'sentMsg:vector(packetBytes)'
    msgSizeVecDescs = vp.getVecDicRecursive(vp.vecDesc, msgSizeVecName)
    recvrAddrVecName = 'recvrAddr:vector'
    recvrAddrVecDescs = vp.getVecDicRecursive(vp.vecDesc, recvrAddrVecName)
    assert len(recvrAddrVecDescs) == len(msgSizeVecDescs)

    hostVecInfo = {hostId : prf.AttrDict() for hostId in
        range(len(msgSizeVecDescs))}

    for i in hostVecInfo:
        msgSizeDesc = msgSizeVecDescs[i]
        moduleName = msgSizeDesc[0]
        matchHostId = re.match('host\[(\d+)\]\.\S+', moduleName)
        assert not(matchHostId == None)
        hostId = int(matchHostId.group(1))
        vecDesc = msgSizeDesc[1]
        hostVecInfo[hostId].msgSizeVecId = vecDesc.vecId

        recvrAddrDesc = recvrAddrVecDescs[i]
        moduleName = recvrAddrDesc[0]
        matchHostId = re.match('host\[(\d+)\]\.\S+', moduleName)
        assert not(matchHostId == None)
        hostId = int(matchHostId.group(1))
        vecDesc = recvrAddrDesc[1]
        hostVecInfo[hostId].recvrAddrVecId = vecDesc.vecId

    trafficData = prf.AttrDict()
    for senderId, vecInfo in hostVecInfo.iteritems():
        sizeVecData = vp.getVecDataByIds(vecInfo.msgSizeVecId)
        recvrAddrVecData = vp.getVecDataByIds(vecInfo.recvrAddrVecId)
        trafficData[senderId] = list()
        for i in range(len(sizeVecData)):
            sizeSample = sizeVecData[i]
            addrSample = recvrAddrVecData[i]
            assert sizeSample[0] == addrSample[0] and\
                sizeSample[1] == addrSample[1]
            arrivalTime = sizeSample[1]
            msgSize = int(sizeSample[2])
            recvrAddr = int(addrSample[2])
            trafficData[senderId].append((arrivalTime, msgSize, recvrAddr))

    simulationParams = parseSimParams(vp.paramDic)
    return trafficData, ipHostId, hostIdIpAddr, simulationParams

class Mesg():

    def __init__(self, msgId, tCreation, sendrIntIp, recvrIntIp, size,
            simParams):
        self.msgId = msgId
        self.tCreation = tCreation
        self.size = size
        self.sizeOnWire, self.txPkts = self.getBytesOnWire(size)
        self.sendrIntIp = sendrIntIp
        self.bytesToSend = size
        self.recvrIntIp = recvrIntIp
        self.bytesToRecv = size
        self.simParams = simParams

        # Each element in this list corresponds to a train of packets sent
        # together [(txStart, txStop, arrivalTimeAtRecvr, totalDataBytesInPkts,
        # [pkt1BytesOneWire, pkt2BytesOnWire, ...]), ...]
        self.pktsInflight = []
        self.pktsRecvd = []
        self.recvTime = tCreation # tracks when the last byte is received
        self.receptionStart = sys.float_info.max # tracks when the
                                                 # first bit is received
        self.minRecvTime, minReceptionStart, pktArrivalsAtHops =\
            self.pktsRecvTime(self.tCreation, [sum(pkt) for pkt in self.txPkts])

    def __lt__(self, other):
        return ((self.bytesToSend, self.size, self.tCreation, self.sendrIntIp,\
        self.recvrIntIp, self.msgId) < (other.bytesToSend, other.size,\
            other.tCreation, other.sendrIntIp, other.recvrIntIp, other.msgId))

    def __eq__(self, other):
        return (not(self.__lt__(other)) and not(other.__lt__(self)))

    def getBytesOnWire(self, numDataBytes):
        bytesOnWire = 0
        pktsOnWire = list()
        maxDataInPkt = MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE\
            - UDP_HEADER_SIZE - HOMA_UNSCHED_HDR
        hdrTotalSize = MAX_ETHERNET_FRAME_SIZE - maxDataInPkt

        numFullPkts = int(numDataBytes / maxDataInPkt)
        bytesOnWire += numFullPkts * MAX_ETHERNET_FRAME_SIZE
        pktsOnWire.append((hdrTotalSize, maxDataInPkt))
        pktsOnWire = pktsOnWire * numFullPkts

        numPartialBytes = numDataBytes - numFullPkts * maxDataInPkt

        # If all numDataBytes fit in full pkts, we should return at this point
        if numFullPkts > 0 and numPartialBytes == 0:
            return bytesOnWire, pktsOnWire

        numPartialBytes += HOMA_UNSCHED_HDR + IP_HEADER_SIZE + UDP_HEADER_SIZE
        partialHdrBytes = hdrTotalSize

        if numPartialBytes < MIN_ETHERNET_PAYLOAD_BYTES:
            partialHdrBytes += (MIN_ETHERNET_PAYLOAD_BYTES - numPartialBytes)
            numPartialBytes = MIN_ETHERNET_PAYLOAD_BYTES

        numPartialBytesOnWire = numPartialBytes + ETHERNET_HDR_SIZE +\
                ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP

        bytesOnWire += numPartialBytesOnWire
        pktsOnWire.append((partialHdrBytes,
            numPartialBytesOnWire-partialHdrBytes))

        return bytesOnWire, pktsOnWire

    def transmitBytes(self, tStart, tStop):
        """
        Returns number of bytes yet to be transmitted for this message after
        call to this function
        """
        txDuration = tStop - tStart
        assert txDuration >= 0
        assert self.bytesToSend > 0
        # For txDuration time, get how many bytes will be transmitted
        bytesAllowedToSend = int(round(
            txDuration * self.simParams.nicLinkSpeed * 1e9 / 8.0))

        inflightPkts = []
        dataBytesSent = 0
        for i in range(len(self.txPkts)):
            if bytesAllowedToSend <= 0:
                break
            txPkt = self.txPkts[i]
            bytesInPkt = sum(txPkt)
            bytesToSendForPkt = min(bytesInPkt, bytesAllowedToSend)
            bytesAllowedToSend -= bytesToSendForPkt
            inflightPkts.append(bytesToSendForPkt)
            hdrBytesToSend = min(bytesToSendForPkt, txPkt[0])
            txPkt = (txPkt[0] - hdrBytesToSend, txPkt[1])
            bytesToSendForPkt -= hdrBytesToSend
            dataBytesToSend = min(bytesToSendForPkt, txPkt[1])
            txPkt = (txPkt[0],  txPkt[1] - dataBytesToSend)
            self.txPkts[i] = txPkt
            dataBytesSent += dataBytesToSend
            self.bytesToSend -= dataBytesToSend

        # Add transmitting packets to inflight pkts
        if inflightPkts:
            if self.pktsInflight and self.pktsInflight[-1][1] == tStart:
                # this new train of packet comes right after previous one (ie.
                # tStart of this train is equal to tStop of prev train), so
                # append this new train of packets to the previous train
                lastInflight = self.pktsInflight[-1]
                prevTxPkts = lastInflight[3]
                newTxPkts =  prevTxPkts[0:-1] +\
                    [(prevTxPkts[-1] + inflightPkts[0])] + inflightPkts[1:]

                newInflight = (lastInflight[0], tStop,
                    lastInflight[2] + dataBytesSent, newTxPkts)

                self.pktsInflight.pop() # remove previous pkts
                self.pktsInflight.append(newInflight) # append new pkts
                                                      # comprised of previous
                                                      # one and new one
            else:
                self.pktsInflight.append(
                    (tStart, tStop, dataBytesSent, inflightPkts))


        # Remove transmitted packets from txPkts
        while self.txPkts and sum(self.txPkts[0]) == 0:
            self.txPkts.pop(0)

        assert self.bytesToSend >= 0
        return self.bytesToSend

    def recvTransmittedPkts(self):
        # For a fully transmitted message, return time at which the message is
        # fully delivered at the receiver.
        assert self.bytesToSend == 0
        dataBytesRecvd = 0
        pktsRecvTimeAtRecvr = self.tCreation
        pktsArrivalsAtHops = None
        for i, pktTrain in enumerate(self.pktsInflight):
            tStart = pktTrain[0]
            tStop = pktTrain[1]
            dataBytesSentInPkt = pktTrain[2]
            inflightPkts = pktTrain[3]
            dataBytesRecvd += dataBytesSentInPkt
            pktsRecvTimeAtRecvr, receptionStart, pktsArrivalsAtHops =\
                self.pktsRecvTime(tStart, inflightPkts, pktsArrivalsAtHops)

            if self.receptionStart == sys.float_info.max:
                self.receptionStart = receptionStart

            assert self.receptionStart <= receptionStart
            self.pktsRecvd.append((tStart, tStop, pktsRecvTimeAtRecvr,
                dataBytesSentInPkt, inflightPkts))

        self.recvTime = pktsRecvTimeAtRecvr
        assert self.bytesToRecv == dataBytesRecvd
        self.bytesToRecv -= dataBytesRecvd

    def getTxDuration(self):
        # return how long does it take to transmit all bytes of this mesg
        txBytesOnWire = sum(self.txPkts)
        return (txBytesOnWire * 8.0 * 1e-9) / self.simParams.nicLinkSpeed

    def pktsRecvTime(self, txStart, txPkts, prevPktArrivals=None):
        """
        For a train of txPkts as [pkt0.byteOnWire, pkt1.byteOnWire,.. ] that
        start transmission at sender at time txStart, this method returns the
        arrival time of last bit of this train at the receiver. prevPktArrivals
        is the arrival times of txPkts train prior this one for this message.
        """
        # Returns the arrival time of last bit of last pkt and first bit of
        # first pakcet in pkts list when pkts are going across the network
        # through links with speeds in linkSpeeds list
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

            firstBitArrival =\
                arrivals[1][-1] - pkts[0] * 8.0 * 1e-9 / linkSpeeds[-1]
            assert firstBitArrival >= 0
            return arrivals[-1][:], firstBitArrival

        if self.recvrIntIp == self.sendrIntIp:
            # When sender and receiver are the same machine
            return txStart

        edgeSwitchFixDelay = self.simParams.switchFixDelay
        fabricSwitchFixDelay = self.simParams.switchFixDelay
        linkDelays = []
        switchFixDelays = []
        linkSpeeds = []
        totalDelay = 0

        if ((self.sendrIntIp >> 8) & 255) == ((self.recvrIntIp >> 8) & 255):
            # receiver and sender are in same rack

            switchFixDelays += [edgeSwitchFixDelay]
            linkDelays += [self.simParams.edgeLinkDelay] * 2

            if not(self.simParams.isFabricCutThrough):
                linkSpeeds = [self.simParams.nicLinkSpeed] * 2
            else:
                # In order to abuse the pktsDeliveryTime function for finding
                # network serializaiton delay when switches are cut through, we
                # need to define linkSpeeds like below
                linkSpeeds = [self.simParams.nicLinkSpeed]

        elif ((self.sendrIntIp >> 16) & 255) == ((self.recvrIntIp >> 16) & 255):
            # receiver and sender are in same pod

            switchFixDelays +=\
                [edgeSwitchFixDelay, fabricSwitchFixDelay, edgeSwitchFixDelay]
            linkDelays +=\
                [self.simParams.edgeLinkDelay, self.simParams.fabricLinkDelay,\
                self.simParams.fabricLinkDelay, self.simParams.edgeLinkDelay]

            if not(self.simParams.isFabricCutThrough):
                linkSpeeds = [self.simParams.nicLinkSpeed,\
                    self.simParams.fabricLinkSpeed,\
                    self.simParams.fabricLinkSpeed, self.simParams.nicLinkSpeed]
            else:
                linkSpeeds = [self.simParams.nicLinkSpeed,
                    self.simParams.fabricLinkSpeed]

        elif ((self.sendrIntIp >> 24) & 255) == ((self.recvrIntIp >> 24) & 255):
            # receiver and sender in two different pod

            switchFixDelays += [edgeSwitchFixDelay, fabricSwitchFixDelay,\
                fabricSwitchFixDelay, fabricSwitchFixDelay, edgeSwitchFixDelay]

            linkDelays +=\
                [self.simParams.edgeLinkDelay, self.simParams.fabricLinkDelay,\
                self.simParams.fabricLinkDelay, self.simParams.fabricLinkDelay,\
                self.simParams.fabricLinkDelay, self.simParams.edgeLinkDelay]

            if not(self.simParams.isFabricCutThrough):
                linkSpeeds = [self.simParams.nicLinkSpeed,\
                    self.simParams.fabricLinkSpeed,\
                    self.simParams.fabricLinkSpeed,\
                    self.simParams.fabricLinkSpeed,\
                    self.simParams.fabricLinkSpeed, self.simParams.nicLinkSpeed]
            else:
                linkSpeeds = [self.simParams.nicLinkSpeed,\
                    self.simParams.fabricLinkSpeed]
        else:
            raise Exception, 'Sender and receiver IPs dont abide the rules'
            ' in config.xml file.'

        # Add network serialization delays at switches and sender nic
        pktArrivalsAtHops, arrivalStart = pktsDeliveryTime(linkSpeeds, txPkts)
        totalDelay += pktArrivalsAtHops[-1]

        # Add fixed delays:
        fixedDelays = self.simParams.hostSwTurnAroundTime * 2 +\
            self.simParams.hostNicSxThinkTime +\
            sum(linkDelays) + sum(switchFixDelays)

        totalDelay += fixedDelays
        arrivalStart +=  fixedDelays

        return totalDelay, arrivalStart, pktArrivalsAtHops

class TrafficData:
    def __init__(self, trafficData, hostIdIpAddr, maxMesgs = sys.maxint):
        self.trafficData = trafficData
        self.hostIdIpAddr = hostIdIpAddr
        self.numInitialMesgs = sum(
            [len(hostTraffic) for hostTraffic in trafficData.values()])

        self.size = min(self.numInitialMesgs, maxMesgs)
        self.onDueMesgs = []
        for hostId in trafficData.keys():
            # Populate onDueMesgs with one mesg from each sender
            self.addToOnDueMesgs(hostId)

    def addToOnDueMesgs(self, hostId):
        if not(self.trafficData[hostId]) or not(self.size):
            del self.trafficData[hostId]
            return
        nextMesg = self.trafficData[hostId].pop(0)
        nextMesg += (hostId,)
        heappush(self.onDueMesgs, nextMesg)

    def getOnDueMesg(self):
        if not(self.onDueMesgs) or not(self.size):
            return None
        onDueMesg = heappop(self.onDueMesgs)
        self.size -= 1
        senderId = onDueMesg[3]
        onDueMesg = list(onDueMesg)
        onDueMesg[3] = self.hostIdIpAddr[senderId][0] # replace senderId with
                                                      # senderIp in the mesg
        self.addToOnDueMesgs(senderId)
        return tuple(onDueMesg) #(mesgCreationTime, msgSize,
                                # recvrIntIp, sendrIntIp)

    def getNextDueTime(self):
        if not(self.onDueMesgs) or not(self.size):
            return sys.float_info.max
        onDueMesg = heappop(self.onDueMesgs)
        dueTime = onDueMesg[0]
        heappush(self.onDueMesgs, onDueMesg)
        return dueTime

    def empty(self):
        assert self.size >= 0
        return not(self.size)

class RecvrState:
    def __init__(self, recvrIntIp, recvrId):
        self.recvrIntIp = recvrIntIp
        self.recvrId = recvrId
        self.numIncompleteMesgs = 0
        self.dataToRecv = 0

        self.activePeriodStart = sys.float_info.max
        self.activePeriodStop = 0.0
        self.activePeriodBytes = 0
        self.sumActivePriods = 0.0

        self.totalSimTime = 0.0
        self.allRecvdBytes = 0
        self.recvdData = 0

    def mesgCreated(self, mesg):
        tCurrent = mesg.tCreation
        if self.numIncompleteMesgs == 0 and self.activePeriodStop != 0.0 and\
                tCurrent > self.activePeriodStop:
            # We have finished an active period, so aggregate the stats and
            # reset the variables for tracking next active period
            assert self.activePeriodStart < self.activePeriodStop and\
                self.dataToRecv == 0 and self.activePeriodBytes > 0 and\
                self.totalSimTime < self.activePeriodStop

            #pdb.set_trace()
            self.sumActivePriods +=\
                (self.activePeriodStop - self.activePeriodStart)
            self.totalSimTime = self.activePeriodStop

            self.activePeriodStart = sys.float_info.max
            self.activePeriodStop = 0.0
            self.activePeriodBytes = 0

        self.numIncompleteMesgs += 1
        self.dataToRecv += mesg.size

    def mesgRecvCompleted(self, mesg):
        self.numIncompleteMesgs -= 1
        self.dataToRecv -= mesg.size
        self.recvdData += mesg.size
        allRecvdBytes = 0.0
        for pktsData in mesg.pktsRecvd:
            allRecvdBytes += sum(pktsData[4])
        assert allRecvdBytes == mesg.sizeOnWire

        self.allRecvdBytes += allRecvdBytes
        self.activePeriodBytes += allRecvdBytes
        self.activePeriodStart =\
            min(self.activePeriodStart, mesg.receptionStart)

        self.activePeriodStop = max(self.activePeriodStop, mesg.recvTime)

    def simFinished(self):
        assert self.numIncompleteMesgs == 0 and self.dataToRecv == 0
        if self.activePeriodStop != 0.0:
            assert self.activePeriodStart < self.activePeriodStop and\
                self.totalSimTime < self.activePeriodStop

            self.sumActivePriods +=\
                (self.activePeriodStop - self.activePeriodStart)

            self.totalSimTime = self.activePeriodStop
            self.activePeriodStart = sys.float_info.max
            self.activePeriodStop = 0.0
            self.activePeriodBytes = 0

class GreedySRPTOracleScheduler():
    def __init__(self, mesgTx, ipHostId, hostIdIpAddr, simParams,
            maxMesgs = sys.maxint):
        self.mesgTx = mesgTx
        self.ipHostId = ipHostId
        self.hostIdIpAddr = hostIdIpAddr
        self.simParams = simParams
        self.t = 0.0 # tracks the global wall clock time in the system
        self.msgId = 0 # monotonically increasing global id for messages
        self.trafficData = TrafficData(mesgTx, hostIdIpAddr, maxMesgs)
        self.numHosts = len(ipHostId)
        self.hostTxStates = [prf.AttrDict() for hostId in range(self.numHosts)]
        self.simResults = []
        self.recvrStates = {}
        for hostId in range(self.numHosts):
            recvrIntIp = self.hostIdIpAddr[hostId][0]
            self.recvrStates[recvrIntIp] = RecvrState(recvrIntIp, hostId)

        for hostId, txState in enumerate(self.hostTxStates):
            # all mesgs that need to transmit bytes at host i
            txState.activeMesgs = {}

        # mesgs that need to transmit but currently not transmitting
        self.activeTxMesgs = []

        # mesgs currently transmitting (ie. last known transmission schedule).
        # schedMesgs and activeTxMesgs are disjoint at any point time and union
        # set of these two contains all messages that need to transmit.
        self.schedMesgs = []

        # Contain messages that have sent all their data. Holds them until they
        # are handled and garbadge collected.
        self.completedTxMesgs = {}

        self.resultFd =\
            open('results/' + self.simParams.workloadType +\
            '__{0:.2f}'.format(self.simParams.loadFactor), 'w')
        self.resultFd.write('msgSize\tmsgSizeOnWire\tsizeHistBin\t'
            'creationTime\tcompletionTime\tstretch\tmsgId\tsendrId'
            '\tsenderIp\trecvrId\trecvrIp\n')

    def runSimulation(self):
        nextPrintTime = 0
        while not(self.trafficData.empty()) or self.hasBytesToTransmit():

            if int(self.t*1e3) == nextPrintTime:
                # every 10ms print a progress report
                nextPrintTime += 10
                activeMesgs = len(self.activeTxMesgs) + len(self.schedMesgs)
                mesgsRemained = self.trafficData.size + activeMesgs
                print('time:{0}, no. mesgs trasmitted: {1},'\
                    ' no. active mesgs: {2}, no. future mesgs: {3}'.format(self.t,
                    self.trafficData.numInitialMesgs - mesgsRemained,
                    activeMesgs, self.trafficData.size))

            # Find next time to run scheduler
            nextMsgTime = self.trafficData.getNextDueTime()
            nextSchedTime = min(nextMsgTime, self.earliestTxCompletion(self.t))
            assert nextSchedTime >= self.t

            # Update state of the system at nextSchedTime for the currently
            # scheduled (transmitting) mesgs
            self.transmitSchedMesgs(self.t, nextSchedTime)

            # Update current time
            self.t = nextSchedTime
            if nextSchedTime == nextMsgTime:
                # create new message and push to the active transmitting
                # messages
                onDueMesg = self.trafficData.getOnDueMesg()
                assert onDueMesg[0] == self.t
                sendrIntIp = onDueMesg[3]
                recvrIntIp = onDueMesg[2]
                mesgSize = onDueMesg[1]
                newMesg = Mesg(self.msgId, self.t, sendrIntIp, recvrIntIp,
                    mesgSize, self.simParams)

                # Get the reciever state and call mesgCreated() for it
                recvrState = self.recvrStates[recvrIntIp]
                recvrState.mesgCreated(newMesg)

                self.msgId += 1
                self.pushActiveTxMesg(newMesg)

            # calculate new schedule
            self.scheduleTransmission()

            # Let all inflight bytes belong to fully transmitted messages to
            # drain out of network and get delivered at receivers
            self.recvTransmittedMesgs()

        # find wasted bandwidth at the receivers
        totalActiveWastedTime = 0.0
        totalActiveTime = 0.0
        totalSimTime = 0.0
        totalRecvBytes = 0.0
        for recvrId, recvrState in self.recvrStates.iteritems():
            recvrState.simFinished()
            wastedTime = recvrState.sumActivePriods - recvrState.allRecvdBytes\
                * 8.0 / (1e9 * self.simParams.nicLinkSpeed)
            if wastedTime < 0:
                print("wasted bw {0} is negative for receiver {1}".format(
                    wastedTime/recvrState.totalSimTime, recvrState.recvrId))
            else:
                totalActiveWastedTime += wastedTime
            totalRecvBytes += recvrState.allRecvdBytes
            totalSimTime += recvrState.totalSimTime
            totalActiveTime += recvrState.sumActivePriods

        self.resultFd.write("# avgRecvBw: {0}Gbps, wasted active bw (Percentage"
            " of total time, Percentage of active time): ({1}, {2})".format(
            totalRecvBytes*8.0/totalSimTime/1e9,
            totalActiveWastedTime*100.0/totalSimTime,
            totalActiveWastedTime*100.0/totalActiveTime))
        self.resultFd.close()

    def scheduleTransmission(self):
        assert not(self.schedMesgs)
        attainableSendrsIPs = set([key for key in self.ipHostId.keys()])
        attainableRecvrsIPs = set([key for key in self.ipHostId.keys()])

        unschedulabeMesgs = []
        mesg = self.popActiveTxMesg()
        while mesg:
            if not(attainableSendrsIPs) or not(attainableRecvrsIPs):
                break
            recvrIp = mesg.recvrIntIp
            sendrIp = mesg.sendrIntIp
            if recvrIp in attainableRecvrsIPs and\
                    sendrIp in attainableSendrsIPs:

                self.schedMesgs.append(mesg)
                attainableSendrsIPs.remove(sendrIp)
                attainableRecvrsIPs.remove(recvrIp)
            else:
                unschedulabeMesgs.append(mesg)

            mesg = self.popActiveTxMesg()

        for mesg in unschedulabeMesgs:
            self.pushActiveTxMesg(mesg)

    def pushActiveTxMesg(self, mesg):
        # Push the mesg to containers for keeping all non-transmitting active
        # for transmission messages and keep the containers ordered.
        heappush(self.activeTxMesgs, mesg)
        hostId = self.ipHostId[mesg.sendrIntIp]
        hostTxState = self.hostTxStates[hostId]
        hostTxState.activeMesgs[mesg.msgId] = mesg

    def popActiveTxMesg(self):
        if not(self.activeTxMesgs):
            return None;
        shortestMesg = heappop(self.activeTxMesgs)
        sendrId = self.ipHostId[shortestMesg.sendrIntIp]
        hostTxState = self.hostTxStates[sendrId]
        del hostTxState.activeMesgs[shortestMesg.msgId]
        return shortestMesg;

    def earliestTxCompletion(self, currentTime):
        earliestTxCompletionTime = sys.float_info.max
        for mesg in self.schedMesgs:
             earliestTxCompletionTime = min(earliestTxCompletionTime,
                mesg.getTxDuration() + currentTime)

        return earliestTxCompletionTime

    def transmitSchedMesgs(self, currentTime, txStopTime):
        # Transmit from currently scheduled messages starting at currentTime
        # for txDuration time period. The side effect of this call is that,
        # messages whose transmission is not finished, will be descheduled and
        # added to the list of all messages waiting for transmission.
        incompleteTxMesgs = []
        while self.schedMesgs:
            mesg = self.schedMesgs.pop(0)
            if mesg.transmitBytes(currentTime, txStopTime):
                incompleteTxMesgs.append(mesg)
            else:
                self.completedTxMesgs[mesg.msgId] = mesg

        for mesg in incompleteTxMesgs:
            self.pushActiveTxMesg(mesg)

    def recvTransmittedMesgs(self):
        recvdMesgIds = []
        for mesgId, mesg in self.completedTxMesgs.iteritems():
            mesg.recvTransmittedPkts()
            # find recvr of this message and call mesgRecvComplete for it
            recvrState = self.recvrStates[mesg.recvrIntIp]
            recvrState.mesgRecvCompleted(mesg)

            histBinInd =\
                bisect.bisect_left(self.simParams.msgSizeRanges, mesg.size)
            histBin =\
                'inf' if histBinInd==len(self.simParams.msgSizeRanges)\
                else self.simParams.msgSizeRanges[histBinInd]
            recordLine =\
                '{0}\t\t{1}\t\t{2}\t\t{3}\t\t{4}\t\t{5}\t\t{6}\t\t{7}'\
                '\t\t{8}\t\t{9}\t\t{10}\n'.format(mesg.size, mesg.sizeOnWire,
                histBin, mesg.tCreation, mesg.recvTime,
                (mesg.recvTime-mesg.tCreation)/
                (mesg.minRecvTime-mesg.tCreation),
                mesg.msgId, self.ipHostId[mesg.sendrIntIp],
                ipIntToStr(mesg.sendrIntIp),
                self.ipHostId[mesg.recvrIntIp], ipIntToStr(mesg.recvrIntIp))
            self.simResults.append(recordLine)

        if len(self.simResults) > 10000 or not(self.hasBytesToTransmit()):
            for recLine in self.simResults:
                self.resultFd.write(recLine)
            del self.simResults[:]

        self.completedTxMesgs.clear()

    def hasBytesToTransmit(self):
        return bool(self.schedMesgs) or bool(self.activeTxMesgs)

if __name__ == '__main__':
    parser = OptionParser(description='')
    parser.add_option('--vec', metavar='VECTOR_RESULT_FILE', default = '',
            dest='vecFile',
            help='Mandatory argument. Full address to vector result file')
    parser.add_option('--vci', metavar='RESULT_INDEX_FILE', default = '',
            dest='vciFile',
            help='Mandatory argument. Full address to index file')
    parser.add_option('--sca', metavar='SCALAR_RESULT_FILE', default = '',
            dest='scaFile',
            help='Mandatory argument. Full address to scalar result file')
    parser.add_option('--maxMesgs', metavar='INT', default = sys.maxint,
            dest='maxMesgs',
            help='A arbitrary upper limit on the number of total messages to'\
            'run the simulation for.')

    options, args = parser.parse_args()
    vecFile = options.vecFile
    vciFile = options.vciFile
    scaFile = options.scaFile
    maxMesgs = int(options.maxMesgs)

    if not(vecFile):
        raise Exception, 'Vector result file name not provided!'
    if not(scaFile):
        raise Exception, 'Scalar result file name not provided!'
    if not(vciFile):
        raise Exception, 'Result index file name not provided!'

    trafficData, ipHostId , hostIdIpAddr, simParams =\
        preprocInputData(vecFile, vciFile, scaFile)
    srptOracle = GreedySRPTOracleScheduler(trafficData,
        ipHostId, hostIdIpAddr, simParams, maxMesgs)

    srptOracle.runSimulation()
