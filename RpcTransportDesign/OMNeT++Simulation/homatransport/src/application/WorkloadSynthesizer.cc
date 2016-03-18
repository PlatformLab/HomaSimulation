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

/**
 * Constructor
 */
WorkloadSynthesizer::WorkloadSynthesizer()
{
    msgSizeGenerator = NULL;
    selfMsg = NULL;
    isSender = false;
    txMsgSize = -1;
}

/**
 * Destructor
 */
WorkloadSynthesizer::~WorkloadSynthesizer()
{
    delete msgSizeGenerator;
}

/**
 * Perform preparaion phases of the module for operation. Includes stuff like
 * Rading parameters and setting up the message size and time interval generator.
 */
void
WorkloadSynthesizer::initialize()
{
    // read in module parameters
    int parentHostIdx = -1;
    nicLinkSpeed = par("nicLinkSpeed").longValue();
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
    }  else if (strcmp(workLoadType, "FABRICATED_HEAVY_HEAD")
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
        cModule* parentHost = this->getParentModule();
        if (strcmp(parentHost->getName(), "host") != 0) {
            throw cRuntimeError("'%s': Not a valid parent module type. Expected"
                    " \"HostBase\" for parent module type.",
                    parentHost->getName());
        }
        parentHostIdx = parentHost->getIndex();
    } else {
        throw cRuntimeError("'%s': Not a valie workload type.",workLoadType);
    }

    HomaPkt homaPkt = HomaPkt();
    homaPkt.setPktType(PktType::UNSCHED_DATA);
    int maxDataBytesPerEthFrame =
            MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE - UDP_HEADER_SIZE;
    int maxDataBytesPerPkt = maxDataBytesPerEthFrame - homaPkt.headerSize();

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

    // Setup timer event generator
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

/**
 * A general demultiplexer function for handling all types of events and
 * messages.
 *
 * \param msg
 *      Represents a timer event or a message that has arrived at the gates of
 *      this module.
 */
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

/**
 * Performs all late stage initalization that must be perfomed after all modules
 * in the network are initialized but before the time that this module can
 * normally operate.
 */
void
WorkloadSynthesizer::processStart()
{
    // Find and set source address for the local host. The assumption here is
    // that each host has only one non-loopback interface and the IP of that
    // interface is srcAddress.
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

    // call parseXml to complete intialization from xml parameter given in
    // config.xml
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

/**
 * As a part of the initialization, this function can be called to parse out the
 * xml paramters in config.xml file
 */
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

/**
 * Randomly chooses a destination among all the destination specified in
 * config.xml for this host.
 *
 * \return
 *      A layer 3 address datastructure containing the IP4 address of the
 *      selected destination
 */
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

/**
 * This method handles the SEND timer event.
 */
void
WorkloadSynthesizer::processSend()
{
    sendMsg();
    setupNextSend();
}

/**
 * Produce a synthesized message of size #txMsgSize and sends it to the
 * RcpHandler module for transmission. It also sends a duplicate of that message
 * to the OracleScheduler module if a connection is setup between them.
 */
void
WorkloadSynthesizer::sendMsg()
{
    inet::L3Address destAddrs = chooseDestAddr();
    char msgName[100];
    sprintf(msgName, "WorkloadSynthesizerMsg-%d", numSent);
    AppMessage *appMessage = new AppMessage(msgName);
    appMessage->setByteLength(txMsgSize);
    appMessage->setDestAddr(destAddrs);
    appMessage->setSrcAddr(srcAddress);
    if (gate("oracleSchedulerOut")->isConnected()) {
        send(appMessage->dup(), "oracleSchedulerOut");
    }
    send(appMessage, "rpcHandlerOut");
    numSent++;
}

/**
 * Sets the #selfMsg timer event for a future transmission.
 */
void
WorkloadSynthesizer::setupNextSend()
{
    double nextSendInterval;
    msgSizeGenerator->getSizeAndInterarrival(txMsgSize, nextSendInterval);
    simtime_t nextSendTime = nextSendInterval + simTime();
    if (txMsgSize < 0 || nextSendTime > stopTime) {
        selfMsg->setKind(STOP);
        scheduleAt(stopTime, selfMsg);
        return;
    }
    ASSERT(selfMsg->getKind() == SelfMsgKinds::SEND);
    scheduleAt(nextSendTime, selfMsg);
}

/*
 * This function is handler for any inbound message received from outside world.
 *
 * \param msg
 *      The pointer to the received message.
 */
void
WorkloadSynthesizer::processRcvdMsg(cPacket* msg)
{
    AppMessage* rcvdMsg = check_and_cast<AppMessage*>(msg);
    uint64_t msgByteLen = (uint64_t)(rcvdMsg->getByteLength());
    EV_INFO << "Received a message of length " << msgByteLen
            << "Bytes" << endl;
    delete rcvdMsg;
    numReceived++;
    return;
}

void
WorkloadSynthesizer::processStop() {
    // Simply return and wait for the inbound messages to arrive.
    return;
}

void
WorkloadSynthesizer::finish()
{
    recordScalar("packets sent", numSent);
    recordScalar("packets received", numReceived);
    cancelAndDelete(selfMsg);
}
