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

#include "WorkloadSynthesizer.h"
#include "application/AppMessage_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/common/InterfaceEntry.h"
#include "inet/networklayer/common/InterfaceTable.h"
#include "inet/networklayer/ipv4/IPv4InterfaceData.h"


Define_Module(WorkloadSynthesizer);

simsignal_t WorkloadSynthesizer::sentMsgSignal = registerSignal("sentMsg");
simsignal_t WorkloadSynthesizer::rcvdMsgSignal = registerSignal("rcvdMsg");

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
    if (strcmp(par("interArrivalDist").stringValue(), "exponential")) {
        interArrivalDist = InterArrivalDist::EXPONENTIAL;
    } else {
        throw cRuntimeError("'%s': Not a valid Interarrival Distribution",
                par("interArrivalDist").stringValue());
    }

    // Calculate average inter arrival time
    avgInterArrivalTime = 10e-9 * msgSizeGenerator->getAvgMsgSize() * 8 /
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

    // Initialize statistics
    numSent = 0;
    numReceived = 0;
    WATCH(numSent);
    WATCH(numReceived);
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
    emit(sentMsgSignal, appMessage);
    send(appMessage, "transportOut");
    numSent++;
}

void
WorkloadSynthesizer::processStart()
{
    const char *destAddrs = par("destAddresses");
    cStringTokenizer tokenizer(destAddrs);
    const char *token;
    while((token = tokenizer.nextToken()) != NULL) {
        inet::L3Address result;
        inet::L3AddressResolver().tryResolve(token, result);
        if (result.isUnspecified())
            EV_ERROR << "cannot resolve destination address: "
                    << token << endl;
        else
            destAddresses.push_back(result);
    }

    if (!destAddresses.empty()) {
        selfMsg->setKind(SEND);
        processSend();
    } else {
        selfMsg->setKind(STOP);
        scheduleAt(stopTime, selfMsg);
    }

    // set srcAddress. The assumption here is that each host has only one
    // non-loopback interface and the IP of that interface is srcAddress.
    inet::InterfaceTable* ifaceTable = 
            check_and_cast<inet::InterfaceTable*>(
            getModuleByPath(par("interfaceTableModule").stringValue()));
    inet::InterfaceEntry* srcIface = NULL;
    inet::IPv4InterfaceData* srcIPv4Data = NULL;
    for (int id=0; id <= ifaceTable->getBiggestInterfaceId(); id++) {
        if ( !(srcIface = ifaceTable->getInterfaceById(id)) &&
                !srcIface->isLoopback() &&
                !(srcIPv4Data = srcIface->ipv4Data())) {
            break;
        }
    }
    if (!srcIface) {
        throw cRuntimeError("Can't find a valid interface on the host");
    } else if (!srcIPv4Data) {
        throw cRuntimeError("Can't find an interface with IPv4 address");
    }
    srcAddress = inet::L3Address(srcIPv4Data->getIPAddress());
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
    switch (interArrivalDist) {
        case InterArrivalDist::EXPONENTIAL:
            return (exponential(avgInterArrivalTime));
        default:
            return 0;
    }
}

void
WorkloadSynthesizer::processRcvdMsg(cPacket* msg)
{
    AppMessage* rcvdMsg = check_and_cast<AppMessage*>(msg);
    emit(rcvdMsgSignal, rcvdMsg);
    EV_INFO << "Received a message of length %d Bytes" <<
            rcvdMsg->getByteLength() << endl;
    delete rcvdMsg;
    numReceived++;
}

void
WorkloadSynthesizer::finish()
{
    recordScalar("packets sent", numSent);
    recordScalar("packets received", numReceived);
}
