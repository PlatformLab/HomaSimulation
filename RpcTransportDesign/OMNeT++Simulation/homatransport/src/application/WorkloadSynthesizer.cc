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
#include "transport/HomaPkt.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/common/InterfaceEntry.h"
#include "inet/networklayer/common/InterfaceTable.h"
#include "inet/networklayer/ipv4/IPv4InterfaceData.h"
Define_Module(WorkloadSynthesizer);

simsignal_t WorkloadSynthesizer::sentMsgSignal = registerSignal("sentMsg");
simsignal_t WorkloadSynthesizer::rcvdMsgSignal = registerSignal("rcvdMsg");
simsignal_t WorkloadSynthesizer::msgE2EDelaySignal =
    registerSignal("msgE2EDelay");
simsignal_t WorkloadSynthesizer::mesgStatsSignal = registerSignal("mesgStats");

WorkloadSynthesizer::WorkloadSynthesizer()
{
    msgSizeGenerator = NULL;
    selfMsg = NULL;
    isSender = false;
    sendMsgSize = -1;
    nextDestHostId = -1;
    hostIdAddrMap.clear();
}

WorkloadSynthesizer::~WorkloadSynthesizer()
{
    delete msgSizeGenerator;
}

void
WorkloadSynthesizer::registerTemplatedStats(const char* msgSizeRanges)
{
    std::stringstream sstream(msgSizeRanges);
    std::istream_iterator<uint64_t> begin(sstream);
    std::istream_iterator<uint64_t> end;
    std::vector<uint64_t> sizeRangeVec(begin, end);
    for (size_t i = 0; i <= sizeRangeVec.size(); i++) {
        std::string sizeUpperBound;
        if (i < sizeRangeVec.size()) {
            sizeUpperBound = std::to_string(sizeRangeVec[i]);
            msgSizeRangeUpperBounds.push_back(sizeRangeVec[i]);
        } else {
            sizeUpperBound = "Huge";
        }

        char latencySignalName[50];
        sprintf(latencySignalName, "msg%sE2EDelay", sizeUpperBound.c_str());
        simsignal_t latencySignal = registerSignal(latencySignalName);
        msgE2ELatencySignalVec.push_back(latencySignal);
        char latencyStatsName[50];
        sprintf(latencyStatsName, "msg%sE2EDelay", sizeUpperBound.c_str());
        cProperty *statisticTemplate =
                getProperties()->get("statisticTemplate", "msgRangesE2EDelay");
        ev.addResultRecorders(this, latencySignal, latencyStatsName,
                statisticTemplate);

        char queueDelaySignalName[50];
        sprintf(queueDelaySignalName, "msg%sQueuingDelay",
                sizeUpperBound.c_str());
        simsignal_t queueDelaySignal = registerSignal(queueDelaySignalName);
        msgQueueDelaySignalVec.push_back(queueDelaySignal);
        char queueDelayStatsName[50];
        sprintf(queueDelayStatsName, "msg%sQueuingDelay",
                sizeUpperBound.c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "msgRangesQueuingDelay");
        ev.addResultRecorders(this, queueDelaySignal, queueDelayStatsName,
                statisticTemplate);

        char stretchSignalName[50];
        sprintf(stretchSignalName, "msg%sE2EStretch", sizeUpperBound.c_str());
        simsignal_t stretchSignal = registerSignal(stretchSignalName);
        msgE2EStretchSignalVec.push_back(stretchSignal);
        char stretchStatsName[50];
        sprintf(stretchStatsName, "msg%sE2EStretch", sizeUpperBound.c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "msgRangesE2EStretch");
        ev.addResultRecorders(this, stretchSignal, stretchStatsName,
                statisticTemplate);

        char transportSchedDelaySignalName[50];
        sprintf(transportSchedDelaySignalName, "msg%sTransportSchedDelay",
            sizeUpperBound.c_str());
        simsignal_t transportSchedDelaySignal =
            registerSignal(transportSchedDelaySignalName);
        msgTransprotSchedDelaySignalVec.push_back(transportSchedDelaySignal);
        char transportSchedDelayStatsName[50];
        sprintf(transportSchedDelayStatsName, "msg%sTransportSchedDelay",
            sizeUpperBound.c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "msgRangesTransportSchedDelay");
        ev.addResultRecorders(this, transportSchedDelaySignal,
            transportSchedDelayStatsName, statisticTemplate);

        char msgBytesOnWireSignalName[50];
        sprintf(msgBytesOnWireSignalName,
            "msg%sBytesOnWire", sizeUpperBound.c_str());
        simsignal_t msgBytesOnWireSignal =
            registerSignal(msgBytesOnWireSignalName);
        msgBytesOnWireSignalVec.push_back(
            msgBytesOnWireSignal);
        char msgBytesOnWireStatsName[50];
        sprintf(msgBytesOnWireStatsName,
            "msg%sBytesOnWire", sizeUpperBound.c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "msgRangesBytesOnWire");
        ev.addResultRecorders(this, msgBytesOnWireSignal,
            msgBytesOnWireStatsName, statisticTemplate);
    }
}

void
WorkloadSynthesizer::initialize()
{
    // read in module parameters
    nicLinkSpeed = par("nicLinkSpeed").longValue();
    fabricLinkSpeed = par("fabricLinkSpeed").longValue();
    edgeLinkDelay = 1e-6 * par("edgeLinkDelay").doubleValue();
    fabricLinkDelay = 1e-6 * par("fabricLinkDelay").doubleValue();
    hostSwTurnAroundTime = 1e-6 * par("hostSwTurnAroundTime").doubleValue();
    hostNicSxThinkTime = 1e-6 * par("hostNicSxThinkTime").doubleValue();
    switchFixDelay = 1e-6 * par("switchFixDelay").doubleValue();
    isFabricCutThrough = par("isFabricCutThrough").boolValue();
    isSingleSpeedFabric = par("isSingleSpeedFabric").boolValue();
    startTime = par("startTime").doubleValue();
    stopTime = par("stopTime").doubleValue();
    xmlConfig = par("appConfig").xmlValue();

    // Setup templated statistics ans signals
    const char* msgSizeRanges = par("msgSizeRanges").stringValue();
    registerTemplatedStats(msgSizeRanges);

    // Find host id for parent host module of this app
    cModule* parentHost = this->getParentModule();
    if (strcmp(parentHost->getName(), "host") != 0) {
        throw cRuntimeError("'%s': Not a valid parent module type. Expected"
                " \"HostBase\" for parent module type.",
                parentHost->getName());
    }
    parentHostIdx = parentHost->getIndex();

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
    } else if (strcmp(workLoadType, "FACEBOOK_WEB_SERVER_INTRACLUSTER")
            == 0) {
        distSelector =
                MsgSizeDistributions::DistributionChoice::
                FACEBOOK_WEB_SERVER_INTRACLUSTER;
        distFileName = std::string("../../sizeDistributions/"
                "Facebook_WebServerDist_IntraCluster.txt");
    } else if (strcmp(workLoadType, "FACEBOOK_CACHE_FOLLOWER_INTRACLUSTER")
            == 0) {
        distSelector =
                MsgSizeDistributions::DistributionChoice::
                FACEBOOK_CACHE_FOLLOWER_INTRACLUSTER;
        distFileName = std::string("../../sizeDistributions/"
                "Facebook_CacheFollowerDist_IntraCluster.txt");
    } else if (strcmp(workLoadType, "GOOGLE_ALL_RPC")
            == 0) {
        distSelector =
                MsgSizeDistributions::DistributionChoice::
                GOOGLE_ALL_RPC;
        distFileName = std::string("../../sizeDistributions/"
                "Google_AllRPC.txt");
    } else if (strcmp(workLoadType, "FACEBOOK_HADOOP_ALL")
            == 0) {
        distSelector =
                MsgSizeDistributions::DistributionChoice::
                FACEBOOK_HADOOP_ALL;
        distFileName = std::string("../../sizeDistributions/"
                "Facebook_HadoopDist_All.txt");
    } else if (strcmp(workLoadType, "FABRICATED_HEAVY_MIDDLE")
            == 0) {
        distSelector =
                MsgSizeDistributions::DistributionChoice::
                FABRICATED_HEAVY_MIDDLE;
        distFileName = std::string("../../sizeDistributions/"
                "Fabricated_Heavy_Middle.txt");
    } else if (strcmp(workLoadType, "GOOGLE_SEARCH_RPC")
            == 0) {
        distSelector =
            MsgSizeDistributions::DistributionChoice::GOOGLE_SEARCH_RPC;
        distFileName = std::string("../../sizeDistributions/"
                "Google_SearchRPC.txt");
    } else if (strcmp(workLoadType, "FABRICATED_HEAVY_HEAD")
            == 0) {
        distSelector =
                MsgSizeDistributions::DistributionChoice::
                FABRICATED_HEAVY_HEAD;
        distFileName = std::string("../../sizeDistributions/"
                "Fabricated_Heavy_Head.txt");
    } else if (strcmp(workLoadType, "PRESET_IN_FILE") == 0){
        distSelector =
                MsgSizeDistributions::DistributionChoice::SIZE_IN_FILE;
        distFileName = std::string(
                "../../sizeDistributions/HostidSizeInterarrival.txt");
    } else {
        throw cRuntimeError("'%s': Not a valie workload type.",workLoadType);
    }

    HomaPkt homaPkt = HomaPkt();
    homaPkt.setPktType(PktType::UNSCHED_DATA);
    maxDataBytesPerEthFrame =
            MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE - UDP_HEADER_SIZE;
    maxDataBytesPerPkt = maxDataBytesPerEthFrame - homaPkt.headerSize();

    MsgSizeDistributions::InterArrivalDist interArrivalDist;
    if (strcmp(par("interArrivalDist").stringValue(), "exponential") == 0) {
        interArrivalDist = MsgSizeDistributions::InterArrivalDist::EXPONENTIAL;
    } else if (strcmp(par("interArrivalDist").stringValue(),
            "preset_in_file") == 0) {
        interArrivalDist =
                MsgSizeDistributions::InterArrivalDist::INTERARRIVAL_IN_FILE;
    } else if (strcmp(par("interArrivalDist").stringValue(),
            "facebook_key_value") == 0){
        interArrivalDist =
                MsgSizeDistributions::InterArrivalDist::FACEBOOK_PARETO;
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
    // dest host be chosen randomly among all the possible dest hosts. Positive
    // dest ids for hosts this app will send to and negative for hosts this app
    // doesn't send to.
    std::string destIdsStr =
            std::string(xmlConfig->getElementByPath("destIds")->getNodeValue());
    std::stringstream destIdsStream(destIdsStr);
    int id;
    std::unordered_set<int> destHostIds;
    while (destIdsStream >> id) {
        if (id == -1) {
            destHostIds.clear();
            destHostIds.insert(id);
            break;
        }
        destHostIds.insert(id);
    }

    const char *destAddrs = par("destAddresses");
    cStringTokenizer tokenizer(destAddrs);
    const char *token;
    while((token = tokenizer.nextToken()) != NULL) {
        cModule* mod = simulation.getModuleByPath(token);
        inet::L3Address result;
        inet::L3AddressResolver().tryResolve(token, result);
        if (result.isUnspecified()) {
            EV_ERROR << "cannot resolve destination address: "
                    << token << endl;
        }
        hostIdAddrMap[mod->getIndex()] = result;

        if (!destHostIds.empty()) {
            if (destHostIds.count(-1)) {
                // Don't include this host (ie loopback) to the set of possible
                // destination hosts
                if (mod->getIndex() == parentHostIdx) {
                    continue;
                }
            } else if (!destHostIds.count(mod->getIndex())) {
                continue;
            }
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
    inet::L3Address destAddrs;
    if (nextDestHostId == -1) {
        destAddrs = chooseDestAddr();
    } else {
        destAddrs = hostIdAddrMap[nextDestHostId];
    }
    char msgName[100];
    sprintf(msgName, "WorkloadSynthesizerMsg-%d", numSent);
    AppMessage *appMessage = new AppMessage(msgName);
    appMessage->setByteLength(sendMsgSize);
    appMessage->setDestAddr(destAddrs);
    appMessage->setSrcAddr(srcAddress);
    appMessage->setMsgCreationTime(appMessage->getCreationTime());
    appMessage->setTransportSchedDelay(appMessage->getCreationTime());
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
    msgSizeGenerator->getSizeAndInterarrival(sendMsgSize, nextDestHostId,
        nextSendInterval);
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
    uint64_t msgByteLen = (uint64_t)(rcvdMsg->getByteLength());
    EV_INFO << "Received a message of length " << msgByteLen
            << "Bytes" << endl;

    double idealDelay = idealMsgEndToEndDelay(rcvdMsg);
    double queuingDelay =  completionTime.dbl() - idealDelay;
    double stretchFactor =
            (idealDelay == 0.0 ? 1.0 : completionTime.dbl()/idealDelay);

    MesgStats mesgStats;
    if (msgByteLen > msgSizeRangeUpperBounds.back())  {
        // if messages size doesn't fit in any bins, then it should be assigned
        // to the last overflow (HugeSize) bin
        emit(msgE2ELatencySignalVec.back(), completionTime);
        emit(msgE2EStretchSignalVec.back(), stretchFactor);
        emit(msgQueueDelaySignalVec.back(), queuingDelay);
        emit(msgTransprotSchedDelaySignalVec.back(),
            rcvdMsg->getTransportSchedDelay());
        emit(msgBytesOnWireSignalVec.back(),
            rcvdMsg->getMsgBytesOnWire());
        mesgStats.mesgSizeBin = UINT64_MAX;
    } else {
        size_t mid, high, low;
        high = msgSizeRangeUpperBounds.size() - 1;
        low = 0;
        while(low < high) {
            mid = (high + low) / 2;
            if (msgByteLen <= msgSizeRangeUpperBounds[mid]) {
                high = mid;
            } else {
                low = mid + 1;
            }
        }
        emit(msgE2ELatencySignalVec[high], completionTime);
        emit(msgE2EStretchSignalVec[high], stretchFactor);
        emit(msgQueueDelaySignalVec[high], queuingDelay);
        emit(msgTransprotSchedDelaySignalVec[high],
            rcvdMsg->getTransportSchedDelay());
        emit(msgBytesOnWireSignalVec[high],
            rcvdMsg->getMsgBytesOnWire());
        mesgStats.mesgSizeBin = msgSizeRangeUpperBounds[high];
    }
    mesgStats.mesgSize = msgByteLen;
    mesgStats.mesgSizeOnWire =  rcvdMsg->getMsgBytesOnWire();
    mesgStats.latency =  completionTime;
    mesgStats.stretch =  stretchFactor;
    mesgStats.queuingDelay =  queuingDelay;
    mesgStats.transportSchedDelay =  rcvdMsg->getTransportSchedDelay();
    emit(mesgStatsSignal, &mesgStats);

    delete rcvdMsg;
    numReceived++;
}

/** The proper working of this part of the code depends on the
 * correct ip assignments based on the config.xml file.
 */
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
    int numFullEthFrame = rcvdMsg->getByteLength() / maxDataBytesPerEthFrame;
    uint32_t lastPartialFrameData =
            rcvdMsg->getByteLength() % maxDataBytesPerEthFrame;

    totalBytesTranmitted = numFullEthFrame *
            (MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_HDR_SIZE +
            ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP);

    if (lastPartialFrameData == 0) {
        if (numFullEthFrame == 0) {
            totalBytesTranmitted = MIN_ETHERNET_FRAME_SIZE +
                    ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP;
            lastPartialFrameLen = totalBytesTranmitted;
        }

    } else {
        if (lastPartialFrameData < (MIN_ETHERNET_PAYLOAD_BYTES -
                IP_HEADER_SIZE - UDP_HEADER_SIZE)) {
            lastPartialFrameLen = MIN_ETHERNET_FRAME_SIZE +
                    ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP;
        } else {
            lastPartialFrameLen = lastPartialFrameData + IP_HEADER_SIZE
                    + UDP_HEADER_SIZE + ETHERNET_HDR_SIZE + ETHERNET_CRC_SIZE
                    + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP;
        }
        totalBytesTranmitted += lastPartialFrameLen;
    }

    double msgSerializationDelay =
            1e-9 * ((totalBytesTranmitted << 3) * 1.0 / nicLinkSpeed);

    // There's always two hostSwTurnAroundTime and one nicThinkTime involved
    // in ideal latency for the overhead.
    double hostDelayOverheads = 2 * hostSwTurnAroundTime + hostNicSxThinkTime;

    // Depending on if the switch model is store-forward (omnet++ default model)
    // or cutthrough (as we implemented), the switch serialization delay would
    // be different. The code snipet below finds how many switch a packet passes
    // through and adds the correct switch delay to total delay based on the
    // switch model.
    double totalSwitchDelay = 0;

    double edgeSwitchFixDelay = switchFixDelay;
    double fabricSwitchFixDelay = switchFixDelay;
    double edgeSwitchSerialDelay = 0;
    double fabricSwitchSerialDelay = 0;

    if (numFullEthFrame != 0) {
        edgeSwitchSerialDelay +=
                (MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_HDR_SIZE +
                ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP) *
                1e-9 * 8 / nicLinkSpeed;

        fabricSwitchSerialDelay += (MAX_ETHERNET_PAYLOAD_BYTES +
                ETHERNET_HDR_SIZE + ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE +
                INTER_PKT_GAP) * 1e-9 * 8 / fabricLinkSpeed;
    } else {
        edgeSwitchSerialDelay += lastPartialFrameLen * 1e-9 * 8 / nicLinkSpeed;
        fabricSwitchSerialDelay +=
                lastPartialFrameLen * 1e-9 * 8 / fabricLinkSpeed;
    }

    if (destAddr.toIPv4().getDByte(2) == srcAddr.toIPv4().getDByte(2)) {

        // src and dest in the same rack
        totalSwitchDelay = edgeSwitchFixDelay;
        if (!isFabricCutThrough) {
            totalSwitchDelay =+ edgeSwitchSerialDelay;
        }

        // Add 2 edge link delays
        totalSwitchDelay += (2 * edgeLinkDelay);

    } else if (destAddr.toIPv4().getDByte(1) == srcAddr.toIPv4().getDByte(1)) {

        // src and dest in the same pod
        totalSwitchDelay =
                edgeSwitchFixDelay +  fabricSwitchFixDelay + edgeSwitchFixDelay;
        if (!isFabricCutThrough) {
            totalSwitchDelay +=
                    (2*fabricSwitchSerialDelay + edgeSwitchSerialDelay);
        } else if (!isSingleSpeedFabric) {
            // have cutthrough but forwarding a packet coming from low
            // speed port to high speed port. There will inevitably be one
            // serialization at low speed.
            totalSwitchDelay += edgeSwitchSerialDelay;
        }

        // Add 2 edge link delays and two fabric link delays
        totalSwitchDelay += (2 * edgeLinkDelay + 2 * fabricLinkDelay);

    } else {
        totalSwitchDelay = edgeSwitchFixDelay +
                           fabricSwitchFixDelay +
                           fabricSwitchFixDelay +
                           fabricSwitchFixDelay +
                           edgeSwitchFixDelay;
        if (!isFabricCutThrough) {
            totalSwitchDelay += (fabricSwitchSerialDelay +
                    fabricSwitchSerialDelay + fabricSwitchSerialDelay +
                    fabricSwitchSerialDelay + edgeSwitchSerialDelay);
        } else if (!isSingleSpeedFabric) {

            totalSwitchDelay += edgeSwitchFixDelay;
        }

        // Add 2 edge link delays and 4 fabric link delays
        totalSwitchDelay += (2 * edgeLinkDelay + 4 * fabricLinkDelay);
    }

    return msgSerializationDelay + totalSwitchDelay + hostDelayOverheads;
}

void
WorkloadSynthesizer::finish()
{
    recordScalar("packets sent", numSent);
    recordScalar("packets received", numReceived);
    cancelAndDelete(selfMsg);
}
