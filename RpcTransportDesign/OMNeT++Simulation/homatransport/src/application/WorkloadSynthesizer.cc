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

WorkloadSynthesizer::WorkloadSynthesizer()
{
    msgSizeGenerator = NULL;
    selfMsg = NULL;
}

WorkloadSynthesizer::~WorkloadSynthesizer()
{
    delete msgSizeGenerator;
}

void
WorkloadSynthesizer::initialize()
{

    // Initialize the msgSizeGenerator
    const char* workLoadType = par("workloadType").stringValue();
    MsgSizeDistributions::DistributionChoice distSelector;
    std::string distFileName;
    if (strcmp(workLoadType, "DCTCP") == 0) {
        distSelector = MsgSizeDistributions::DistributionChoice::DCTCP;
        distFileName = std::string(
                "../../sizeDistributions/DCTCP_MsgSizeDist.txt");
    } else if (strcmp(workLoadType, "FACEBOOK_KEy_VALUE")) {
        distSelector = 
                MsgSizeDistributions::DistributionChoice::FACEBOOK_KEY_VALUE;
        distFileName = std::string(
                "../../sizeDistributions/FacebookKeyValueMsgSizeDist.txt");
    } else {
        throw cRuntimeError("'%s': Not a valie workload type.",workLoadType);
    }
    
    maxDataBytesPerPkt = 
            MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE - UDP_HEADER_SIZE; 
    try {
        msgSizeGenerator = 
                new MsgSizeDistributions(distFileName.c_str(), distSelector,
                maxDataBytesPerPkt);

    } catch(MsgSizeDistException& e) {
        throw cRuntimeError("'%s': Not a valid workload type.",workLoadType);
    }
    
    // Set Interarrival Time Distributions type from what's specified
    // in omnetpp.ini file
    if (strcmp(par("interArrivalDist").stringValue(), "exponential") == 0) {
        interArrivalDist = InterArrivalDist::EXPONENTIAL;
    } else {
        throw cRuntimeError("'%s': Not a valid Interarrival Distribution",
                par("interArrivalDist").stringValue());
    }

    // Calculate average inter arrival time
    avgInterArrivalTime = 1e-9 * msgSizeGenerator->getAvgMsgSize() * 8 /
            (par("loadFactor").doubleValue() * par("linkSpeed").longValue());

    // Send timer settings
    startTime = par("startTime").doubleValue();
    stopTime = par("stopTime").doubleValue();
    if (stopTime >= SIMTIME_ZERO && stopTime < startTime)
        throw cRuntimeError("Invalid startTime/stopTime parameters");

    selfMsg = new cMessage("sendTimer");
    simtime_t start = std::max(startTime, simTime());
    if ((stopTime < SIMTIME_ZERO) || (start < stopTime)) {
        selfMsg->setKind(START);
        scheduleAt(start, selfMsg);
    }

    // set xmlConfig 
    xmlConfig = par("appConfig").xmlValue();

    // Initialize statistics
    numSent = 0;
    numReceived = 0;
    WATCH(numSent);
    WATCH(numReceived);
}

void
WorkloadSynthesizer::parseAndProcessXMLConfig()
{
    // determine if this app is also a sender or only a receiver
    bool isSender;
    const char* isSenderParam = 
            xmlConfig->getElementByPath("isSender")->getNodeValue();
    if (strcmp(isSenderParam, "true") == 0) {
        isSender = true;
    } else if (strcmp(isSenderParam, "false") == 0) {
        isSender = false;
    } else {
        throw cRuntimeError("'%s': Not a valid xml parameter for appConfig.",
                isSenderParam);
    }

    if (!isSender) {
        selfMsg->setKind(STOP);
        scheduleAt(stopTime, selfMsg);
        return;
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

    if (destAddresses.empty()) {
        selfMsg->setKind(STOP);
        scheduleAt(stopTime, selfMsg);
    } else {
        selfMsg->setKind(SEND);
        processSend();
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
    int msgByteSize = msgSizeGenerator->sizeGeneratorWrapper();
    char msgName[100];
    sprintf(msgName, "WorkloadSynthesizerMsg-%d", numSent);
    AppMessage *appMessage = new AppMessage(msgName);
    appMessage->setByteLength(msgByteSize);
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
}

void
WorkloadSynthesizer::processStop() {
}

void
WorkloadSynthesizer::processSend()
{
    sendMsg();
    simtime_t nextSend = simTime() + nextSendTime();
    if (stopTime < SIMTIME_ZERO || nextSend < stopTime) {
        selfMsg->setKind(SEND);
        scheduleAt(nextSend, selfMsg);
    } else {
        selfMsg->setKind(STOP);
        scheduleAt(stopTime, selfMsg);
    }
}

double
WorkloadSynthesizer::nextSendTime()
{
    double nextSxTime;
    switch (interArrivalDist) {
        case InterArrivalDist::EXPONENTIAL:
             nextSxTime = exponential(avgInterArrivalTime);
            return (nextSxTime);
        default:
            return 0;
    }
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

    if (msgByteLen <= maxDataBytesPerPkt) {
        emit(msg1PktE2EDelaySignal, completionTime);

    } else if (msgByteLen <= 3 * maxDataBytesPerPkt) {
        emit(msg3PktsE2EDelaySignal, completionTime);

    } else if (msgByteLen <= 6 * maxDataBytesPerPkt) {
        emit(msg6PktsE2EDelaySignal, completionTime);

    } else if (msgByteLen <= 13 * maxDataBytesPerPkt) {
        emit(msg13PktsE2EDelaySignal, completionTime);

    } else if (msgByteLen <= 33 * maxDataBytesPerPkt) {
        emit(msg33PktsE2EDelaySignal, completionTime);

    } else if (msgByteLen <= 133 * maxDataBytesPerPkt) {
        emit(msg133PktsE2EDelaySignal, completionTime);

    } else if (msgByteLen <= 1333 * maxDataBytesPerPkt) {
        emit(msg1333PktsE2EDelaySignal, completionTime);

    } else {
        emit(msgHugeE2EDelaySignal, completionTime);
    }

    delete rcvdMsg;
    numReceived++;
}

double
WorkloadSynthesizer::idealMsgEndToEndDelay(AppMessage* rcvdMsg)
{

    // calculate the total transmitted bytes in the the network for this
    // rcvdMsg. These bytes include all headers and ethernet overhead bytes per
    // frame.
    int totalBytesTranmitted = 0;
    int lastPartialFrameLen = 0;
    int numFullEthFrame = rcvdMsg->getByteLength() / maxDataBytesPerPkt;
    int lastPartialFrameData = rcvdMsg->getByteLength() % maxDataBytesPerPkt;


    totalBytesTranmitted = numFullEthFrame *
            (MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_HDR_SIZE +
            ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE);

    if (lastPartialFrameData == 0) {
        if (numFullEthFrame == 0) {
            totalBytesTranmitted = MIN_ETHERNET_FRAME_SIZE + ETHERNET_PREAMBLE_SIZE;
        }
    } else {
        int minEthernetPayloadSize = 
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
            (1e-9 * (totalBytesTranmitted << 3)) / par("linkSpeed").longValue();

    // The switch models in omnet++ are store and forward therefor the first
    // packet of each message will experience an extra serialialization delay at
    // each switch. There for need to figure out how many switched a packet will
    // pass through.

    
}

void
WorkloadSynthesizer::finish()
{
    recordScalar("packets sent", numSent);
    recordScalar("packets received", numReceived);
    cancelAndDelete(selfMsg);
}
