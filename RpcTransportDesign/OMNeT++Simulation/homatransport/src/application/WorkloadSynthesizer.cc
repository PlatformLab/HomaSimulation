//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include <unordered_set>
#include<iterator>
#include<iostream>
#include "WorkloadSynthesizer.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/common/InterfaceEntry.h"
#include "inet/networklayer/common/InterfaceTable.h"
#include "inet/networklayer/ipv4/IPv4InterfaceData.h"
Define_Module(WorkloadSynthesizer);

simsignal_t WorkloadSynthesizer::sentMsgSignal = registerSignal("sentMsg");
simsignal_t WorkloadSynthesizer::rcvdMsgSignal = registerSignal("rcvdMsg");
simsignal_t WorkloadSynthesizer::msgE2EDelaySignal =
        registerSignal("msgE2EDelay");
/*
simsignal_t WorkloadSynthesizer::msg1PktE2EDelaySignal = 
        registerSignal("msg1PktE2EDelay");
simsignal_t WorkloadSynthesizer::msg3PktsE2EDelaySignal = 
        registerSignal("msg3PktsE2EDelay");
simsignal_t WorkloadSynthesizer::msg6PktsE2EDelaySignal = 
        registerSignal("msg6PktsE2EDelay");
simsignal_t WorkloadSynthesizer::msg13PktsE2EDelaySignal = 
        registerSignal("msg13PktsE2EDelay");
simsignal_t WorkloadSynthesizer::msg33PktsE2EDelaySignal = 
        registerSignal("msg33PktsE2EDelay");
simsignal_t WorkloadSynthesizer::msg133PktsE2EDelaySignal = 
        registerSignal("msg133PktsE2EDelay");
simsignal_t WorkloadSynthesizer::msg1333PktsE2EDelaySignal = 
        registerSignal("msg1333PktsE2EDelay");
simsignal_t WorkloadSynthesizer::msgHugeE2EDelaySignal = 
        registerSignal("msgHugeE2EDelay");

simsignal_t WorkloadSynthesizer::msg1PktE2EStretchSignal = 
        registerSignal("msg1PktE2EStretch");
simsignal_t WorkloadSynthesizer::msg3PktsE2EStretchSignal = 
        registerSignal("msg3PktsE2EStretch");
simsignal_t WorkloadSynthesizer::msg6PktsE2EStretchSignal = 
        registerSignal("msg6PktsE2EStretch");
simsignal_t WorkloadSynthesizer::msg13PktsE2EStretchSignal = 
        registerSignal("msg13PktsE2EStretch");
simsignal_t WorkloadSynthesizer::msg33PktsE2EStretchSignal = 
        registerSignal("msg33PktsE2EStretch");
simsignal_t WorkloadSynthesizer::msg133PktsE2EStretchSignal = 
        registerSignal("msg133PktsE2EStretch");
simsignal_t WorkloadSynthesizer::msg1333PktsE2EStretchSignal = 
        registerSignal("msg1333PktsE2EStretch");
simsignal_t WorkloadSynthesizer::msgHugeE2EStretchSignal = 
        registerSignal("msgHugeE2EStretch");

simsignal_t WorkloadSynthesizer::msg1PktQueuingDelaySignal = 
        registerSignal("msg1PktQueuingDelay");
simsignal_t WorkloadSynthesizer::msg3PktsQueuingDelaySignal = 
        registerSignal("msg3PktsQueuingDelay");
simsignal_t WorkloadSynthesizer::msg6PktsQueuingDelaySignal = 
        registerSignal("msg6PktsQueuingDelay");
simsignal_t WorkloadSynthesizer::msg13PktsQueuingDelaySignal = 
        registerSignal("msg13PktsQueuingDelay");
simsignal_t WorkloadSynthesizer::msg33PktsQueuingDelaySignal = 
        registerSignal("msg33PktsQueuingDelay");
simsignal_t WorkloadSynthesizer::msg133PktsQueuingDelaySignal = 
        registerSignal("msg133PktsQueuingDelay");
simsignal_t WorkloadSynthesizer::msg1333PktsQueuingDelaySignal = 
        registerSignal("msg1333PktsQueuingDelay");
simsignal_t WorkloadSynthesizer::msgHugeQueuingDelaySignal = 
        registerSignal("msgHugeQueuingDelay");
*/


WorkloadSynthesizer::WorkloadSynthesizer()
{
    msgSizeGenerator = NULL;
    selfMsg = NULL;
    isSender = false;
    sendMsgSize = -1;
    msgSizeRanges = NULL;
}

WorkloadSynthesizer::~WorkloadSynthesizer()
{
    delete msgSizeGenerator;
}

void
WorkloadSynthesizer::registerTemplatedStats()
{
    msgSizeRanges = "1Pkt 3Pkts 6Pkts 13Pkts 33Pkts 133Pkts 1333Pkts Huge";
    std::stringstream sstream(msgSizeRanges);
    std::istream_iterator<std::string> begin(sstream);
    std::istream_iterator<std::string> end;
    std::vector<std::string> sizeRangeVec(begin, end);
    for (std::vector<std::string>::iterator it = sizeRangeVec.begin();
            it != sizeRangeVec.end(); ++it) {
        char latencySignalName[50];
        sprintf(latencySignalName, "msg%sE2EDelay", (*it).c_str());
        simsignal_t latencySignal = registerSignal(latencySignalName);
        msgE2ELatencySignalVec.push_back(latencySignal); 
        char latencyStatsName[50];
        sprintf(latencyStatsName, "msg%sE2EDelay", (*it).c_str());
        cProperty *statisticTemplate = 
                getProperties()->get("statisticTemplate", "msgRangesE2EDelay");
        ev.addResultRecorders(this, latencySignal, latencyStatsName, statisticTemplate);

        char queueDelaySignalName[50];
        sprintf(queueDelaySignalName, "msg%sQueuingDelay", (*it).c_str());
        simsignal_t queueDelaySignal = registerSignal(queueDelaySignalName);
        msgQueueDelaySignalVec.push_back(queueDelaySignal); 
        char queueDelayStatsName[50];
        sprintf(queueDelayStatsName, "msg%sQueuingDelay", (*it).c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "msgRangesQueuingDelay");
        ev.addResultRecorders(this, queueDelaySignal, queueDelayStatsName, statisticTemplate);


        char stretchSignalName[50];
        sprintf(stretchSignalName, "msg%sE2EStretch", (*it).c_str());
        simsignal_t stretchSignal = registerSignal(stretchSignalName);
        msgE2EStretchSignalVec.push_back(stretchSignal); 
        char stretchStatsName[50];
        sprintf(stretchStatsName, "msg%sE2EStretch", (*it).c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "msgRangesE2EStretch");
        ev.addResultRecorders(this, stretchSignal, stretchStatsName, statisticTemplate);
       
    }
}

void
WorkloadSynthesizer::initialize()
{   
    // read in module parameters
    int parentHostIdx = -1;
    nicLinkSpeed = par("nicLinkSpeed").longValue();
    fabricLinkSpeed = par("fabricLinkSpeed").longValue(); 
    edgeLinkDelay = 1e-6 * par("edgeLinkDelay").doubleValue();
    fabricLinkDelay = 1e-6 * par("fabricLinkDelay").doubleValue();
    nicThinkTime = 1e-6 * par("nicThinkTime").doubleValue(); 
    startTime = par("startTime").doubleValue();
    stopTime = par("stopTime").doubleValue();
    xmlConfig = par("appConfig").xmlValue();

    // Initialize the msgSizeGenerator
    const char* workLoadType = par("workloadType").stringValue();
    MsgSizeDistributions::DistributionChoice distSelector;
    std::string distFileName;
    if (strcmp(workLoadType, "DCTCP") == 0) {
        distSelector = MsgSizeDistributions::DistributionChoice::DCTCP;
        distFileName = std::string(
                "../../sizeDistributions/DCTCP_MsgSizeDist.txt");
    } else if (strcmp(workLoadType, "TEST_DIST") == 0) {
        distSelector = MsgSizeDistributions::DistributionChoice::TEST_DIST;
        distFileName = std::string(
                "../../sizeDistributions/TestDistribution.txt");
    } else if (strcmp(workLoadType, "FACEBOOK_KEY_VALUE") == 0) {
        distSelector = 
                MsgSizeDistributions::DistributionChoice::FACEBOOK_KEY_VALUE;
        distFileName = std::string(
                "../../sizeDistributions/FacebookKeyValueMsgSizeDist.txt");
    } else if (strcmp(workLoadType, "PRESET_IN_FILE") == 0){
        distSelector = 
                MsgSizeDistributions::DistributionChoice::SIZE_IN_FILE;
        distFileName = std::string(
                "../../sizeDistributions/HostidSizeInterarrival.txt");
        cModule* parentHost = this->getParentModule();
        if (strcmp(parentHost->getName(), "HostBase") != 0) {
            throw cRuntimeError("'%s': Not a valid parent module type. Expected "
                    "\"HostBase\" for parent module type.", parentHost->getName());
        }
        parentHostIdx = parentHost->getIndex();
    } else {
        throw cRuntimeError("'%s': Not a valie workload type.",workLoadType);
    }
    
    maxDataBytesPerPkt = 
            MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE - UDP_HEADER_SIZE; 

    MsgSizeDistributions::InterArrivalDist interArrivalDist;
    if (strcmp(par("interArrivalDist").stringValue(), "exponential") == 0) {
        interArrivalDist = MsgSizeDistributions::InterArrivalDist::EXPONENTIAL;
    } else if (strcmp(par("interArrivalDist").stringValue(), "preset_in_file") == 0) {
        interArrivalDist = MsgSizeDistributions::InterArrivalDist::INTERARRIVAL_IN_FILE;
    } else {
        throw cRuntimeError("'%s': Not a valid Interarrival Distribution",
                par("interArrivalDist").stringValue());
    }

    double avgRate = par("loadFactor").doubleValue() * nicLinkSpeed;
    msgSizeGenerator = new MsgSizeDistributions(distFileName.c_str(),
            maxDataBytesPerPkt, interArrivalDist, distSelector, avgRate,
            parentHostIdx);

    // Send timer settings
    if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
        throw cRuntimeError("Invalid startTime/stopTime parameters");

    selfMsg = new cMessage("sendTimer");
    simtime_t start = std::max(startTime, simTime());
    if ((stopTime < SIMTIME_ZERO) || (start < stopTime)) {
        selfMsg->setKind(START);
        scheduleAt(start, selfMsg);
    }

    if (stopTime < SIMTIME_ZERO) {
        stopTime = MAXTIME;
    }

    // Initialize statistic tracker variables
    numSent = 0;
    numReceived = 0;
    WATCH(numSent);
    WATCH(numReceived);

    registerTemplatedStats();
}

void
WorkloadSynthesizer::parseAndProcessXMLConfig()
{
    // determine if this app is also a sender or only a receiver
    const char* isSenderParam = 
            xmlConfig->getElementByPath("isSender")->getNodeValue();
    if (strcmp(isSenderParam, "false") == 0) {
        isSender = false;
        return;
    } else if (strcmp(isSenderParam, "true") == 0) {
        isSender = true;
    } else {
        throw cRuntimeError("'%s': Not a valid xml parameter for appConfig.",
                isSenderParam);
    }


    // destAddress will be populated with the destination hosts in the xml
    // config file. If no destination is specified in the xml config file, then
    // dest host be chosen randomly among all the possible dest hosts.
    std::string destIdsStr =
            std::string(xmlConfig->getElementByPath("destIds")->getNodeValue());
    std::stringstream destIdsStream(destIdsStr); 
    int id;
    std::unordered_set<int> destHostIds;
    while (destIdsStream >> id) {
        destHostIds.insert(id);
    }

    const char *destAddrs = par("destAddresses");
    cStringTokenizer tokenizer(destAddrs);
    const char *token;
    while((token = tokenizer.nextToken()) != NULL) {
        cModule* mod = simulation.getModuleByPath(token);
        if (!destHostIds.empty() && destHostIds.count(mod->getIndex()) == 0) {
                continue;
        }
        inet::L3Address result;
        inet::L3AddressResolver().tryResolve(token, result);
        if (result.isUnspecified()) {
            EV_ERROR << "cannot resolve destination address: "
                    << token << endl;
        }
        destAddresses.push_back(result);
    }
}

void
WorkloadSynthesizer::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        ASSERT(msg == selfMsg);
        switch(msg->getKind()) {
            case START:
                processStart();
                break;
            case SEND:
                processSend();
                break;
            case STOP:
                processStop();
                break;
            default:
                throw cRuntimeError("Invalid kind %d in self message",
                        (int)selfMsg->getKind());  
        }
    } else {
        processRcvdMsg(check_and_cast<AppMessage*>(msg));
    }
}

inet::L3Address
WorkloadSynthesizer::chooseDestAddr()
{
    int k = intrand(destAddresses.size());
    if (destAddresses[k].isLinkLocal()) {    // KLUDGE for IPv6
        const char *destAddrs = par("destAddresses");
        cStringTokenizer tokenizer(destAddrs);
        const char *token = NULL;

        for (int i = 0; i <= k; ++i)
            token = tokenizer.nextToken();
        destAddresses[k] = inet::L3AddressResolver().resolve(token);
    }

    EV_DETAIL << "Selected destination address: " << destAddresses[k].str()
            << " among " << destAddresses.size() <<" available destinations.\n";
    return destAddresses[k];
}

void
WorkloadSynthesizer::sendMsg()
{
    inet::L3Address destAddrs = chooseDestAddr();
    char msgName[100];
    sprintf(msgName, "WorkloadSynthesizerMsg-%d", numSent);
    AppMessage *appMessage = new AppMessage(msgName);
    appMessage->setByteLength(sendMsgSize);
    appMessage->setDestAddr(destAddrs);
    appMessage->setSrcAddr(srcAddress);
    appMessage->setMsgCreationTime(0);
    emit(sentMsgSignal, appMessage);
    send(appMessage, "transportOut");
    numSent++;
}

void
WorkloadSynthesizer::processStart()
{
    // set srcAddress. The assumption here is that each host has only one
    // non-loopback interface and the IP of that interface is srcAddress.
    inet::InterfaceTable* ifaceTable = 
            check_and_cast<inet::InterfaceTable*>(
            getModuleByPath(par("interfaceTableModule").stringValue()));
    inet::InterfaceEntry* srcIface = NULL;
    inet::IPv4InterfaceData* srcIPv4Data = NULL;
    for (int i=0; i < ifaceTable->getNumInterfaces(); i++) {
        if ((srcIface = ifaceTable->getInterface(i)) &&
                !srcIface->isLoopback() &&
                (srcIPv4Data = srcIface->ipv4Data())) {
            break;
        }
    }
    if (!srcIface) {
        throw cRuntimeError("Can't find a valid interface on the host");
    } else if (!srcIPv4Data) {
        throw cRuntimeError("Can't find an interface with IPv4 address");
    }
    srcAddress = inet::L3Address(srcIPv4Data->getIPAddress());

    // call parseXml to complete intialization based on the config.xml file
    parseAndProcessXMLConfig();

    // Start the Sender application based on the parsed xmlConfig results
    // If this app is not sender or is a sender but no receiver is available for
    // this sender, then just set the sendTimer to the stopTime and only wait
    // for message arrivals.
    if (!isSender || destAddresses.empty()) {
        selfMsg->setKind(STOP);
        scheduleAt(stopTime, selfMsg);
        return;
    }

    selfMsg->setKind(SEND);
    setupNextSend();
}

void
WorkloadSynthesizer::processStop() {
}

void
WorkloadSynthesizer::processSend()
{
    sendMsg();
    setupNextSend();
}

void
WorkloadSynthesizer::setupNextSend()
{
    double nextSendInterval;
    msgSizeGenerator->getSizeAndInterarrival(sendMsgSize, nextSendInterval);
    simtime_t nextSendTime = nextSendInterval + simTime();
    if (sendMsgSize < 0 || nextSendTime > stopTime) {
        selfMsg->setKind(STOP);
        scheduleAt(stopTime, selfMsg);
        return;
    }
    ASSERT(selfMsg->getKind() == SelfMsgKinds::SEND);
    scheduleAt(nextSendTime, selfMsg);
}

void
WorkloadSynthesizer::processRcvdMsg(cPacket* msg)
{
    AppMessage* rcvdMsg = check_and_cast<AppMessage*>(msg);
    emit(rcvdMsgSignal, rcvdMsg);
    simtime_t completionTime = simTime() - rcvdMsg->getMsgCreationTime();
    emit(msgE2EDelaySignal, completionTime);
    int msgByteLen = rcvdMsg->getByteLength();
    EV_INFO << "Received a message of length " << msgByteLen
            << "Bytes" << endl;

    double idealDelay = idealMsgEndToEndDelay(rcvdMsg);
    double queuingDelay =  completionTime.dbl() - idealDelay;
    double stretchFactor = 
            (idealDelay == 0.0 ? 1.0 : completionTime.dbl()/idealDelay);

    if (msgByteLen <= maxDataBytesPerPkt) {
        emit(msgE2ELatencySignalVec[0], completionTime);
        emit(msgE2EStretchSignalVec[0], stretchFactor);
        emit(msgQueueDelaySignalVec[0], queuingDelay);

    } else if (msgByteLen <= 3 * maxDataBytesPerPkt) {
        emit(msgE2ELatencySignalVec[1], completionTime);
        emit(msgE2EStretchSignalVec[1], stretchFactor);
        emit(msgQueueDelaySignalVec[1], queuingDelay);

    } else if (msgByteLen <= 6 * maxDataBytesPerPkt) {
        emit(msgE2ELatencySignalVec[2], completionTime);
        emit(msgE2EStretchSignalVec[2], stretchFactor);
        emit(msgQueueDelaySignalVec[2], queuingDelay);

    } else if (msgByteLen <= 13 * maxDataBytesPerPkt) {
        emit(msgE2ELatencySignalVec[3], completionTime);
        emit(msgE2EStretchSignalVec[3], stretchFactor);
        emit(msgQueueDelaySignalVec[3], queuingDelay);

    } else if (msgByteLen <= 33 * maxDataBytesPerPkt) {
        emit(msgE2ELatencySignalVec[4], completionTime);
        emit(msgE2EStretchSignalVec[4], stretchFactor);
        emit(msgQueueDelaySignalVec[4], queuingDelay);

    } else if (msgByteLen <= 133 * maxDataBytesPerPkt) {
        emit(msgE2ELatencySignalVec[5], completionTime);
        emit(msgE2EStretchSignalVec[5], stretchFactor);
        emit(msgQueueDelaySignalVec[5], queuingDelay);

    } else if (msgByteLen <= 1333 * maxDataBytesPerPkt) {
        emit(msgE2ELatencySignalVec[6], completionTime);
        emit(msgE2EStretchSignalVec[6], stretchFactor);
        emit(msgQueueDelaySignalVec[6], queuingDelay);

    } else {
        emit(msgE2ELatencySignalVec[7], completionTime);
        emit(msgE2EStretchSignalVec[7], stretchFactor);
        emit(msgQueueDelaySignalVec[7], queuingDelay);
    }

    delete rcvdMsg;
    numReceived++;
}

double
WorkloadSynthesizer::idealMsgEndToEndDelay(AppMessage* rcvdMsg)
{
    int totalBytesTranmitted = 0;
    inet::L3Address srcAddr = rcvdMsg->getSrcAddr();
    ASSERT(srcAddr.getType() == inet::L3Address::AddressType::IPv4);
    inet::L3Address destAddr = rcvdMsg->getDestAddr();
    ASSERT(destAddr.getType() == inet::L3Address::AddressType::IPv4);
    
    if (destAddr == srcAddr) {
        // no switching delay
        return totalBytesTranmitted;
    }


    // calculate the total transmitted bytes in the the network for this
    // rcvdMsg. These bytes include all headers and ethernet overhead bytes per
    // frame.
    int lastPartialFrameLen = 0;
    int numFullEthFrame = rcvdMsg->getByteLength() / maxDataBytesPerPkt;
    uint32_t lastPartialFrameData = rcvdMsg->getByteLength() % maxDataBytesPerPkt;


    totalBytesTranmitted = numFullEthFrame *
            (MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_HDR_SIZE +
            ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE);

    if (lastPartialFrameData == 0) {
        if (numFullEthFrame == 0) {
            totalBytesTranmitted = MIN_ETHERNET_FRAME_SIZE + ETHERNET_PREAMBLE_SIZE;
            lastPartialFrameLen = totalBytesTranmitted;
        }

    } else {
        uint32_t minEthernetPayloadSize = 
                MIN_ETHERNET_FRAME_SIZE - ETHERNET_HDR_SIZE - ETHERNET_CRC_SIZE;
        if (lastPartialFrameData < 
                (minEthernetPayloadSize - IP_HEADER_SIZE - UDP_HEADER_SIZE)) {

            lastPartialFrameLen =
                    MIN_ETHERNET_FRAME_SIZE + ETHERNET_PREAMBLE_SIZE;
        } else {
            lastPartialFrameLen = lastPartialFrameData + IP_HEADER_SIZE
                    + UDP_HEADER_SIZE + ETHERNET_HDR_SIZE + ETHERNET_CRC_SIZE
                    + ETHERNET_PREAMBLE_SIZE; 
        }

        totalBytesTranmitted += lastPartialFrameLen; 
    }

    double msgSerializationDelay = 
            1e-9 * ((totalBytesTranmitted << 3) * 1.0 / nicLinkSpeed);

    // The switch models in omnet++ are store and forward therefor the first
    // packet of each message will experience an extra serialialization delay at
    // each switch. Therefore need to figure out how many switched a packet will
    // pass through. The proper working of this part of the code depends on the
    // correct ip assignments.
    double totalSwitchDelay = 0;

    // There's always one nicThinkTime involved ideal latency for software
    // overhead.
    totalSwitchDelay += nicThinkTime;                                
    double edgeSwitchingDelay = 0; 
    double fabricSwitchinDelay = 0;

    if (numFullEthFrame == 0) {
        edgeSwitchingDelay = (MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_HDR_SIZE +
                ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE) *
                1e-9 * 8 / nicLinkSpeed; 

        fabricSwitchinDelay = (MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_HDR_SIZE +
                ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE) *
                1e-9 * 8 / fabricLinkSpeed;
    } else {
        edgeSwitchingDelay = lastPartialFrameLen * 1e-9 * 8 / nicLinkSpeed; 
        fabricSwitchinDelay = lastPartialFrameLen * 1e-9 * 8 / fabricLinkSpeed;
    }

    if (destAddr.toIPv4().getDByte(2) == srcAddr.toIPv4().getDByte(2)) {
        // src and dest in the same rack
        totalSwitchDelay = edgeSwitchingDelay;

        // Add 2 edge link delays
        totalSwitchDelay += (2 * edgeLinkDelay);

    } else if (destAddr.toIPv4().getDByte(1) == srcAddr.toIPv4().getDByte(1)) {
        // src and dest in the same pod 
        totalSwitchDelay = edgeSwitchingDelay + 2 * fabricSwitchinDelay;

        // Add 2 edge link delays and two fabric link delays
        totalSwitchDelay += (2 * edgeLinkDelay + 2 * fabricLinkDelay);

    } else {
        totalSwitchDelay = edgeSwitchingDelay + 4 * fabricSwitchinDelay;

        // Add 2 edge link delays and 4 fabric link delays
        totalSwitchDelay += (2 * edgeLinkDelay + 4 * fabricLinkDelay);


    }

    return msgSerializationDelay + totalSwitchDelay;
}

void
WorkloadSynthesizer::finish()
{
    recordScalar("packets sent", numSent);
    recordScalar("packets received", numReceived);
    cancelAndDelete(selfMsg);
}
