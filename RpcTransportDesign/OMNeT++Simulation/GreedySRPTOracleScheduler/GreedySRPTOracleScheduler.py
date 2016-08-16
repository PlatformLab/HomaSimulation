#!/usr/bin/python

"""
This scripts simulates a central scheduler that has global information about
all messages that enter the network at every sender. It uses an SRPT policy to
find a nonconflicting transmission schedule from senders to receivers.
"""

import sys
import os
import re
from heapq import *
from numpy import *
from glob import glob
from optparse import OptionParser
from pprint import pprint
import pdb

sys.path.insert(0, os.environ['HOME'] + '/Research/RpcTransportDesign/OMNeT++Simulation/analysis')
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
    Input arg is simulation paramter dic parsed out by the parseResultFile module.
    Returns simulation parameter list such that its usable by the components of
    GreedySRPTOracleScheduler module (ie. component defined in this file.)
    """
    paramDic = prf.AttrDict()
    paramDic.nicLinkSpeed = int(simParams.nicLinkSpeed.strip('Gpbs'))
    paramDic.fabricLinkSpeed = int(simParams.fabricLinkSpeed.strip('Gbps'))
    paramDic.numTors = int(simParams.numTors)
    paramDic.numServersPerTor = int(simParams.numServersPerTor)
    paramDic.edgeLinkDelay = float(simParams.edgeLinkDelay.strip('us'))*1e-6
    paramDic.fabricLinkDelay = float(simParams.fabricLinkDelay.strip('us'))*1e-6
    paramDic.hostNicSxThinkTime = float(simParams.hostNicSxThinkTime.strip('us'))*1e-6
    paramDic.hostSwTurnAroundTime = float(simParams.hostSwTurnAroundTime.strip('us'))*1e-6
    paramDic.isFabricCutThrough = simParams.isFabricCutThrough == 'true'
    paramDic.switchFixDelay = float(simParams.switchFixDelay.strip('us'))*1e-6
    paramDic.msgSizeRanges = [float(strSizeBound) for strSizeBound in simParams.msgSizeRanges.strip('\"').split()]
    paramDic.workloadType = simParams.workloadType 
    paramDic.loadFactor = 100*float(simParams.loadFactor)
    return paramDic

def preprocInputData(vecFile, vciFile, scaFile):
    """
    Inputs are the file names for vector and scalar result files that contain
    the traffice data from simulation, along with the index file name from
    simulation. The output is parsed traffice data and simulation information
    for this module.
    """
    sp = prf.ScalarParser(scaFile)
    numHosts = int(sp.generalInfo.numTors) * int(sp.generalInfo.numServersPerTor)

    # IP addr of host with id equal to 'i' is at index i of the list. IPs are
    # recorded as tuples of int and string values: (int, 'YYY.YYY.YYY.YYY')
    hostIdIpAddr = list()

    # Dictionary that maps the integer values of host IP address to hostId
    ipHostId = dict()

    for hostId in range(numHosts):
         ipInt = int(sp.hosts.access('host[{0}].trafficGeneratorApp[0]."Host IPv4 address".value'.format(hostId)))
         ipStr = ipIntToStr(ipInt)
         hostIdIpAddr.append((ipInt, ipStr))
         ipHostId[ipInt] = hostId


    vp = prf.VectorParser(vciFile, vecFile)
    msgSizeVecName = 'sentMsg:vector(packetBytes)'
    msgSizeVecDescs = vp.getVecDicRecursive(vp.vecDesc, msgSizeVecName)
    recvrAddrVecName = 'recvrAddr:vector'
    recvrAddrVecDescs = vp.getVecDicRecursive(vp.vecDesc, recvrAddrVecName)
    assert len(recvrAddrVecDescs) == len(msgSizeVecDescs)

    hostVecInfo = {hostId : prf.AttrDict() for hostId in range(len(msgSizeVecDescs))}
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
            assert sizeSample[0] == addrSample[0] and sizeSample[1] == addrSample[1]
            arrivalTime = sizeSample[1]
            msgSize = int(sizeSample[2])
            recvrAddr = int(addrSample[2])
            trafficData[senderId].append((arrivalTime, msgSize, recvrAddr))

    simulationParams = parseSimParams(vp.paramDic)
    return trafficData, ipHostId, hostIdIpAddr, simulationParams

class Mesg():

    def __init__(self, msgId, tCreation, sendrIntIp, recvrIntIp, size, simParams):
        self.msgId = msgId
        self.tCreation = tCreation
        self.size = size
        self.sizeOnWire, self.txPkts = self.getBytesOnWire(size)
        self.sendrIntIp = sendrIntIp
        self.bytesToSend = size
        self.recvrIntIp = recvrIntIp
        self.bytesToRecv = size
        self.simParams = simParams

        # Each element in this list corresponds to a train of packets sent together
        # [(arrivalTimeAtRecvr, totalDataBytesInPkts, [pkt1BytesOneWire, pkt2BytesOnWire, ...]), ...]
        self.pktsInflight = []
        self.pktsRecvd = []
        self.recvTime = tCreation
        self.minRecvTime =\
            self.pktsRecvTime(self.tCreation, [sum(pkt) for pkt in self.txPkts])

    def __lt__(self, other):
        return ((self.bytesToSend, self.size, self.tCreation, self.sendrIntIp,\
        self.recvrIntIp, self.msgId) < (other.bytesToSend, other.size, other.tCreation,\
        other.sendrIntIp, other.recvrIntIp, other.msgId))

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
        pktsOnWire.append((partialHdrBytes, numPartialBytesOnWire-partialHdrBytes))
        return bytesOnWire, pktsOnWire

    def transmitBytes(self, txDuration, currentTime):
        """
        Returns number of bytes yet to be transmitted for this message after
        call to this function
        """
        assert txDuration >= 0
        assert self.bytesToSend > 0
        # For txDuration time, get how many bytes will be transmitted
        bytesAllowedToSend =  int(round(txDuration * self.simParams.nicLinkSpeed * 1e9 / 8.0))
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

        if inflightPkts:
            recvTimeAtRx = self.pktsRecvTime(currentTime, inflightPkts)
            self.pktsInflight.append((recvTimeAtRx, dataBytesSent, inflightPkts))

        # Remove transmitted packets from txPkts
        while self.txPkts and sum(self.txPkts[0]) == 0:
            self.txPkts.pop(0)

        assert self.bytesToSend >= 0
        return self.bytesToSend

    def recvBytes(self, currentTime):
        # Returns number of data bytes yet to receiver after this func. call
        # returns
        pktsStillInflight = []
        for i, pktTrain in enumerate(self.pktsInflight):
            arrivalTime = pktTrain[0]
            if arrivalTime <= currentTime:
                dataBytes = pktTrain[1]
                self.recvTime = max(self.recvTime, arrivalTime)
                self.bytesToRecv -= dataBytes
                self.pktsRecvd.append(pktTrain)
            else:
                pktsStillInflight.append(pktTrain)

        self.pktsInflight[:] = pktsStillInflight
        assert self.bytesToRecv >= 0
        return self.bytesToRecv

    def getTxDuration(self):
        # return how long does it take to transmit all bytes of this mesg
        txBytesOnWire = sum(self.txPkts)
        return (txBytesOnWire * 8.0 * 1e-9) / self.simParams.nicLinkSpeed

    def pktsRecvTime(self, txStart, txPkts):
        """
        For a train of txPkts as [pkt0.byteOnWire, pkt1.byteOnWire,.. ] that start transmission
        at sender at time txStart, this method returns the arrival time of last bit of this
        train at the receiver.
        """
        if self.recvrIntIp == self.sendrIntIp:
            # When sender and receiver are the same machine
            return txStart

        edgeSwitchFixDelay = self.simParams.switchFixDelay
        fabricSwitchFixDelay = self.simParams.switchFixDelay
        linkDelays = []
        switchFixDelays = []
        totalDelay = 0

        # n is index of pkt in pkts list, k is index of hops starting at zero
        # for first switch near sender and last being the destination host.
        pktNArrivalTimeAtHopK =\
            lambda linkSpeeds, pkts, n, k : 0 if k < 0 or n < 0 else\
            pkts[n] * 8.0 * 1e-9 / linkSpeeds[k] +\
            max(pktNArrivalTimeAtHopK(linkSpeeds, pkts, n, k-1),\
            pktNArrivalTimeAtHopK(linkSpeeds, pkts, n-1, k))

        if ((self.sendrIntIp >> 8) & 255) == ((self.recvrIntIp >> 8) & 255):
            # receiver and sender are in same rack

            linkSpeeds = [self.simParams.nicLinkSpeed] * 2
            numHops = 2 # includes num switches plus the destination
            switchFixDelays += [edgeSwitchFixDelay]
            linkDelays += [self.simParams.edgeLinkDelay] * 2

            if not(self.simParams.isFabricCutThrough):
                totalDelay += pktNArrivalTimeAtHopK(linkSpeeds, txPkts, len(txPkts)-1, numHops-1)
            else:
                #Abusing the pktNArrivalTimeAtHopK function
                totalDelay +=\
                    pktNArrivalTimeAtHopK([self.simParams.nicLinkSpeed], txPkts,\
                    len(txPkts)-1, 0)

        elif ((self.sendrIntIp >> 16) & 255) == ((self.recvrIntIp >> 16) & 255):
            # receiver and sender are in same pod
            linkSpeeds =\
                [self.simParams.nicLinkSpeed, self.simParams.fabricLinkSpeed,\
                self.simParams.fabricLinkSpeed, self.simParams.nicLinkSpeed]
            numHops = 4 # includes num switches plus the destination
            switchFixDelays +=\
                [edgeSwitchFixDelay, fabricSwitchFixDelay, edgeSwitchFixDelay]
            linkDelays +=\
                [self.simParams.edgeLinkDelay, self.simParams.fabricLinkDelay,\
                self.simParams.fabricLinkDelay, self.simParams.edgeLinkDelay]

            if not(self.simParams.isFabricCutThrough):
                totalDelay += pktNArrivalTimeAtHopK(linkSpeeds, txPkts, len(txPkts)-1, numHops-1)
            else:
                #Abusing the pktNArrivalTimeAtHopK function
                totalDelay +=\
                    pktNArrivalTimeAtHopK([self.simParams.nicLinkSpeed,\
                    self.simParams.fabricLinkSpeed], txPkts, len(txPkts)-1, 1)

        elif ((self.sendrIntIp >> 24) & 255) == ((self.recvrIntIp >> 24) & 255):
            # receiver and sender in two different pod
            linkSpeeds =\
                [self.simParams.nicLinkSpeed, self.simParams.fabricLinkSpeed,\
                self.simParams.fabricLinkSpeed, self.simParams.fabricLinkSpeed,\
                self.simParams.fabricLinkSpeed, self.simParams.nicLinkSpeed]
            numHops = 6 # includes num switches plus the destination
            switchFixDelays +=\
                [edgeSwitchFixDelay, fabricSwitchFixDelay, fabricSwitchFixDelay,\
                fabricSwitchFixDelay, edgeSwitchFixDelay]
            linkDelays +=\
                [self.simParams.edgeLinkDelay, self.simParams.fabricLinkDelay,\
                self.simParams.fabricLinkDelay, self.simParams.fabricLinkDelay,\
                self.simParams.fabricLinkDelay, self.simParams.edgeLinkDelay]

            if not(self.simParams.isFabricCutThrough):
                totalDelay += pktNArrivalTimeAtHopK(linkSpeeds, txPkts, len(txPkts)-1, numHops-1)
            else:
                #Abusing the pktNArrivalTimeAtHopK function
                totalDelay +=\
                    pktNArrivalTimeAtHopK([self.simParams.nicLinkSpeed,\
                    self.simParams.fabricLinkSpeed], txPkts, len(txPkts)-1, 1)
        else:
            raise Exception, 'Sender and receiver IPs dont abide the rules in config.xml file.'

        # Add fixed delays:
        totalDelay +=\
            self.simParams.hostSwTurnAroundTime * 2 + self.simParams.hostNicSxThinkTime +\
            sum(linkDelays) + sum(switchFixDelays)

        return txStart + totalDelay

class TrafficData:
    def __init__(self, trafficData, hostIdIpAddr):
        self.trafficData = trafficData
        self.hostIdIpAddr = hostIdIpAddr
        self.size = sum([len(hostTraffic) for hostTraffic in trafficData.values()])
        self.onDueMesgs = []
        for hostId in trafficData.keys():
            # Populate onDueMesgs with one mesg from each sender
            self.addToOnDueMesgs(hostId)

    def addToOnDueMesgs(self, hostId):
        if not(self.trafficData[hostId]):
            del self.trafficData[hostId]
            return
        nextMesg = self.trafficData[hostId].pop(0)
        nextMesg += (hostId,)
        heappush(self.onDueMesgs, nextMesg)

    def getOnDueMesg(self):
        if not(self.onDueMesgs):
            return None
        onDueMesg = heappop(self.onDueMesgs)
        self.size -= 1
        senderId = onDueMesg[3]
        onDueMesg = list(onDueMesg)
        onDueMesg[3] = self.hostIdIpAddr[senderId][0] # replace senderId with senderIp in the mesg
        self.addToOnDueMesgs(senderId)
        return tuple(onDueMesg) #(mesgCreationTime, msgSize, recvrIntIp, sendrIntIp)

    def getNextDueTime(self):
        if not(self.onDueMesgs):
            return sys.float_info.max
        onDueMesg = heappop(self.onDueMesgs)
        dueTime = onDueMesg[0]
        heappush(self.onDueMesgs, onDueMesg)
        return dueTime

    def empty(self):
        assert self.size >= 0
        return not(self.size)

class GreedySRPTOracleScheduler():
    def __init__(self, mesgTx, ipHostId, hostIdIpAddr, simParams):
        self.mesgTx = mesgTx
        self.ipHostId = ipHostId
        self.hostIdIpAddr = hostIdIpAddr
        self.simParams = simParams
        self.t = 0.0 # tracks the global wall clock time in the system
        self.msgId = 0 # monotonically increasing global id for messages
        self.trafficData = TrafficData(mesgTx, hostIdIpAddr)
        self.numHosts = len(ipHostId)
        self.hostTxStates = [prf.AttrDict() for hostId in range(self.numHosts)]

        for hostId, txState in enumerate(self.hostTxStates):
            # all mesgs that need to transmit bytes at host i
            txState.activeMesgs = {}

        # mesgs that need to transmit but currently not transmitting
        self.activeTxMesgs = []

        # mesgs currently transmitting (ie. last known transmission schedule).
        # schedMesgs and activeTxMesgs are disjoint at any point time and union
        # set of these two contains all messages that need to transmit.
        self.schedMesgs = []

        # all messages that have data to be received
        self.activeRxMesgs = {}

        self.resultFd =\
            open(self.simParams.workloadType + '_{0:.2f}'.format(self.simParams.loadFactor), 'w')
        self.resultFd.write('msgSize\tcreationTime\tcompletionTime\tstretch'
            '\tmsgId\tsendrId\tsenderIp\trecvrId\trecvrIp\n')

    def runSimulation(self):
        pdb.set_trace()
        while not(self.trafficData.empty()) or self.hasBytesToTransmit():
            print self.t
            # Find next time to run scheduling
            nextMsgTime = self.trafficData.getNextDueTime()
            nextSchedTime = min(nextMsgTime, self.earliestTxCompletion(self.t))
            txDuration = nextSchedTime - self.t
            assert txDuration >= 0

            # Update state of the system at nextSchedTime for the currently
            # scheduled (transmitting) mesgs and actively being received messages
            self.transmitSchedMesgs(self.t, txDuration)
            self.recvInfilghtBytes(self.t)

            # Update current time
            self.t = nextSchedTime
            if nextSchedTime == nextMsgTime:
                # create new message and push to the active transmitting messages
                onDueMesg = self.trafficData.getOnDueMesg()
                assert onDueMesg[0] == self.t
                sendrIntIp = onDueMesg[3]
                recvrIntIp = onDueMesg[2]
                mesgSize = onDueMesg[1]
                newMesg = Mesg(self.msgId, self.t, sendrIntIp, recvrIntIp, mesgSize, self.simParams)
                self.msgId += 1
                self.pushActiveTxMesg(newMesg)

            # calculate new schedule
            self.scheduleTransmission()

        # Set the current time to infinity and let all inflight bytes to drain out
        # of network and get delivered at receivers
        self.recvInfilghtBytes(sys.float_info.max)

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
            if recvrIp in attainableRecvrsIPs and sendrIp in attainableSendrsIPs:
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
             earliestTxCompletionTime =\
                min(earliestTxCompletionTime, mesg.getTxDuration() + currentTime)
        return earliestTxCompletionTime

    def transmitSchedMesgs(self, currentTime, txDuration):
        # Transmit from currently scheduled messages starting at currentTime
        # for txDuration time period. The side effect of this call is that,
        # messages whose transmission is not finished, will be unscheduled and
        # added to the list of all messages waiting for transmission.
        incompleteTxMesgs = []
        while self.schedMesgs:
            mesg = self.schedMesgs.pop(0)
            self.activeRxMesgs[mesg.msgId] = mesg
            if mesg.transmitBytes(txDuration, currentTime):
                incompleteTxMesgs.append(mesg)

        for mesg in incompleteTxMesgs:
            self.pushActiveTxMesg(mesg)

    def recvInfilghtBytes(self, currentTime):
        recvdMesgIds = []
        for mesgId, mesg in self.activeRxMesgs.iteritems():
            if not(mesg.recvBytes(currentTime)):
                recvdMesgIds.append(mesgId)
        for recvdMesgId in recvdMesgIds:
            mesg = self.activeRxMesgs[recvdMesgId]
            recordLine = '{0}\t{1}\t{2}\t{3:.2f}\t{4}\t{5}\t{6}\t{7}\t{8}\n'.format(
                mesg.size, mesg.tCreation, mesg.recvTime,
                (mesg.recvTime-mesg.tCreation)/(mesg.minRecvTime-mesg.tCreation),
                mesg.msgId, self.ipHostId[mesg.sendrIntIp], ipIntToStr(mesg.sendrIntIp),
                self.ipHostId[mesg.recvrIntIp], ipIntToStr(mesg.recvrIntIp))
            self.resultFd.write(recordLine)
            del self.activeRxMesgs[recvdMesgId]

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

    options, args = parser.parse_args()
    vecFile = options.vecFile
    vciFile = options.vciFile
    scaFile = options.scaFile
    if not(vecFile):
        raise Exception, 'Vector result file name not provided!'
    if not(scaFile):
        raise Exception, 'Scalar result file name not provided!'
    if not(vciFile):
        raise Exception, 'Result index file name not provided!'

    trafficData, ipHostId , hostIdIpAddr, simParams =\
        preprocInputData(vecFile, vciFile, scaFile)
    srptOracle = GreedySRPTOracleScheduler(trafficData, ipHostId, hostIdIpAddr, simParams)
    srptOracle.runSimulation()
