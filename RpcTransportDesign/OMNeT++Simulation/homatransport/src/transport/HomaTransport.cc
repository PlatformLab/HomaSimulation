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

#include <algorithm>
#include <random>
#include <cmath>
#include "transport/HomaPkt.h"
#include "transport/TrafficPacer.h"
#include "HomaTransport.h"

Define_Module(HomaTransport);

/**
 * Registering all of the statistics collection signals.
 */
simsignal_t HomaTransport::msgsLeftToSendSignal =
        registerSignal("msgsLeftToSend");
simsignal_t HomaTransport::bytesLeftToSendSignal =
        registerSignal("bytesLeftToSend");
simsignal_t HomaTransport::outstandingGrantBytesSignal =
        registerSignal("outstandingGrantBytes");
simsignal_t HomaTransport::totalOutstandingBytesSignal =
        registerSignal("totalOutstandingBytes");

/**
 * Contstructor for the HomaTransport.
 */
HomaTransport::HomaTransport()
    : sxController(this)
    , rxScheduler(this)
    , prioResolver(NULL)
    , distEstimator(NULL)
    , socket()
    , localAddr()
    , selfMsg(NULL)
{}

/**
 * Destructor of HomaTransport.
 */
HomaTransport::~HomaTransport()
{
    delete prioResolver;
    delete distEstimator;
}

void
HomaTransport::registerTemplatedStats(uint16_t numPrio)
{
    for (size_t i = 0; i < numPrio; i++) {
        char prioSignalName[50];
        sprintf(prioSignalName, "homaPktPrio%luSignal", i);
        simsignal_t pktPrioSignal = registerSignal(prioSignalName);
        priorityStatsSignals.push_back(pktPrioSignal);
        char pktPrioStatsName[50];
        sprintf(pktPrioStatsName, "homaPktPrio%luSignal", i);
        cProperty *statisticTemplate =
                getProperties()->get("statisticTemplate", "pktPrioStats");
        ev.addResultRecorders(this, pktPrioSignal, pktPrioStatsName,
                statisticTemplate);
    }
}

/**
 *
 */
void
HomaTransport::initialize()
{
    localPort = par("localPort");
    destPort = par("destPort");
    nicLinkSpeed = par("nicLinkSpeed");
    maxOutstandingRecvBytes = par("maxOutstandingRecvBytes");
    uint32_t grantMaxBytes = (uint32_t) par("grantMaxBytes");
    uint16_t allPrio = (uint16_t)par("prioLevels");
    uint16_t schedPrioLevels = (uint16_t)par("schedPrioLevels");
    uint16_t prioResolverPrioLevels = (uint16_t)par("prioResolverPrioLevels");
    const char* schedPrioAssignMode = par("schedPrioAssignMode");
    const char* unschedPrioResolutionMode = par("unschedPrioResolutionMode");
    HomaPkt dataPkt = HomaPkt();
    dataPkt.setPktType(PktType::SCHED_DATA);
    uint32_t maxDataBytes = MAX_ETHERNET_PAYLOAD_BYTES -
        IP_HEADER_SIZE - UDP_HEADER_SIZE - dataPkt.headerSize();
    if (grantMaxBytes > maxDataBytes) {
        grantMaxBytes = maxDataBytes;
    }
    selfMsg = new cMessage("GrantTimer");
    selfMsg->setKind(SelfMsgKind::START);
    registerTemplatedStats(allPrio);

    HomaTransport::ReceiveScheduler::QueueType rxInboundQueueType;
    if (par("isRoundRobinScheduler").boolValue()) {
        rxInboundQueueType =
            HomaTransport::ReceiveScheduler::QueueType::FIFO_QUEUE;
    } else {
        rxInboundQueueType =
            HomaTransport::ReceiveScheduler::QueueType::PRIO_QUEUE;
    }

    distEstimator = new WorkloadEstimator(par("workloadType").stringValue());
    prioResolver = new PriorityResolver(prioResolverPrioLevels, distEstimator);
    rxScheduler.initialize(grantMaxBytes, nicLinkSpeed, allPrio,
        schedPrioLevels, selfMsg, rxInboundQueueType, schedPrioAssignMode,
        prioResolver);
    sxController.initSendController((uint32_t) par("defaultReqBytes"),
            (uint32_t) par("defaultUnschedBytes"), prioResolver,
            prioResolver->strPrioModeToInt(unschedPrioResolutionMode));
    scheduleAt(simTime(), selfMsg);
    outstandingGrantBytes = 0;
}

void
HomaTransport::processStart()
{
    socket.setOutputGate(gate("udpOut"));
    socket.bind(localPort);
    selfMsg->setKind(SelfMsgKind::GRANT);
}

void
HomaTransport::processGrantTimer()
{
    rxScheduler.sendAndScheduleGrant();
}

void
HomaTransport::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        ASSERT(msg == selfMsg);
        switch (msg->getKind()) {
            case SelfMsgKind::START:
                processStart();
                break;
            case SelfMsgKind::GRANT:
                processGrantTimer();
                break;
        }
    } else {
        if (msg->arrivedOn("appIn")) {
            AppMessage* outMsg = check_and_cast<AppMessage*>(msg);
            // check and set the localAddr
            if (localAddr == inet::L3Address()) {
                localAddr = outMsg->getSrcAddr();
            } else {
                ASSERT(localAddr == outMsg->getSrcAddr());
            }
            sxController.processSendMsgFromApp(outMsg);

        } else if (msg->arrivedOn("udpIn")) {
            HomaPkt* rxPkt = check_and_cast<HomaPkt*>(msg);
            // check and set the localAddr
            if (localAddr == inet::L3Address()) {
                localAddr = rxPkt->getDestAddr();
            } else {
                ASSERT(localAddr == rxPkt->getDestAddr());
            }
            // update the owner transport for this packet
            rxPkt->ownerTransport = this;
            emit(priorityStatsSignals[rxPkt->getPriority()], rxPkt);
            switch (rxPkt->getPktType()) {
                case PktType::REQUEST:
                    rxScheduler.processReceivedRequest(rxPkt);
                    break;

                case PktType::UNSCHED_DATA:
                    rxScheduler.processReceivedUnschedData(rxPkt);
                    break;

                case PktType::GRANT:
                    sxController.processReceivedGrant(rxPkt);
                    break;

                case PktType::SCHED_DATA:
                    rxScheduler.processReceivedSchedData(rxPkt);
                    break;

                default:
                    throw cRuntimeError(
                        "Received packet type(%d) is not valid.",
                        rxPkt->getPktType());
            }
        }
    }
}

void
HomaTransport::sendPacket(HomaPkt* sxPkt)
{
    socket.sendTo(sxPkt, sxPkt->getDestAddr(), destPort);
}

void
HomaTransport::finish()
{
    cancelAndDelete(selfMsg);
}


/**
 * HomaTransport::SendController
 */
HomaTransport::SendController::SendController(HomaTransport* transport)
    : transport(transport)
    , bytesLeftToSend(0)
    , msgId(0)
    , outboundMsgMap()
    , unschedByteAllocator(NULL)
    , prioResolver(NULL)
    , unschedPrioResMode()
{}

void
HomaTransport::SendController::initSendController(uint32_t defaultReqBytes,
    uint32_t defaultUnschedBytes, PriorityResolver* prioResolver,
    PriorityResolver::PrioResolutionMode unschedPrioResMode)
{
    this->prioResolver = prioResolver;
    this->unschedPrioResMode = unschedPrioResMode;
    bytesLeftToSend = 0;
    std::random_device rd;
    std::mt19937_64 merceneRand(rd());
    std::uniform_int_distribution<uint64_t> dist(0, UINTMAX_MAX);
    msgId = dist(merceneRand);
    unschedByteAllocator =
            new UnschedByteAllocator(defaultReqBytes, defaultUnschedBytes);
}

HomaTransport::SendController::~SendController()
{
    delete unschedByteAllocator;
}

void
HomaTransport::SendController::processSendMsgFromApp(AppMessage* sendMsg)
{
    transport->emit(msgsLeftToSendSignal, outboundMsgMap.size());
    transport->emit(bytesLeftToSendSignal, bytesLeftToSend);
    uint32_t destAddr =
            sendMsg->getDestAddr().toIPv4().getInt();
    uint32_t msgSize = sendMsg->getByteLength();
    uint32_t dataBytesInReq =
            unschedByteAllocator->getReqDataBytes(destAddr, msgSize);
    uint32_t unschedDataBytes =
            unschedByteAllocator->getUnschedBytes(destAddr, msgSize);
    outboundMsgMap.emplace(std::piecewise_construct,
            std::forward_as_tuple(msgId),
            std::forward_as_tuple(sendMsg, this, msgId, dataBytesInReq,
            unschedDataBytes));
    outboundMsgMap.at(msgId).sendRequestAndUnsched();
    if (outboundMsgMap.at(msgId).bytesLeft <= 0) {
        outboundMsgMap.erase(msgId);
    } else {
        bytesLeftToSend += outboundMsgMap.at(msgId).bytesLeft;
    }
    msgId++;
    delete sendMsg;
}

void
HomaTransport::SendController::processReceivedGrant(HomaPkt* rxPkt)
{
    EV_INFO << "Received a grant from " << rxPkt->getSrcAddr().str()
            << " for  msgId " << rxPkt->getMsgId() << " for "
            << rxPkt->getGrantFields().grantBytes << "  Bytes." << endl;

    OutboundMessage* outboundMsg = &(outboundMsgMap.at(rxPkt->getMsgId()));
    ASSERT(rxPkt->getDestAddr() == outboundMsg->srcAddr &&
            rxPkt->getSrcAddr() == outboundMsg->destAddr);

    int bytesLeftOld = outboundMsg->bytesLeft;
    int bytesLeftNew = outboundMsg->sendSchedBytes(
        rxPkt->getGrantFields().grantBytes, rxPkt->getGrantFields().schedPrio);
    int bytesSent = bytesLeftOld - bytesLeftNew;
    bytesLeftToSend -= bytesSent;
    ASSERT(bytesSent > 0 && bytesLeftToSend >= 0);

    if (bytesLeftNew <= 0) {
        outboundMsgMap.erase(rxPkt->getMsgId());
    }
    delete rxPkt;
}

/**
 * HomaTransport::OutboundMessage
 */
HomaTransport::OutboundMessage::OutboundMessage(AppMessage* outMsg,
        SendController* sxController, uint64_t msgId, uint32_t dataBytesInReq,
        uint32_t unschedDataBytes)
    : sxController(sxController)
    , msgId(msgId)
    , msgSize(outMsg->getByteLength())
    , bytesLeft(outMsg->getByteLength())
    , nextByteToSend(0)
    , dataBytesInReq(dataBytesInReq)
    , unschedDataBytes(unschedDataBytes)
    , destAddr(outMsg->getDestAddr())
    , srcAddr(outMsg->getSrcAddr())
    , msgCreationTime(outMsg->getMsgCreationTime())
{}

HomaTransport::OutboundMessage::OutboundMessage()
    : sxController(NULL)
    , msgId(0)
    , msgSize(0)
    , bytesLeft(0)
    , nextByteToSend(0)
    , dataBytesInReq(0)
    , unschedDataBytes(0)
    , destAddr()
    , srcAddr()
    , msgCreationTime(SIMTIME_ZERO)
{}

HomaTransport::OutboundMessage::~OutboundMessage()
{}

HomaTransport::OutboundMessage::OutboundMessage(const OutboundMessage& other)
{
    copy(other);
}

HomaTransport::OutboundMessage&
HomaTransport::OutboundMessage::operator=(const OutboundMessage& other)
{
    if (this==&other) return *this;
    copy(other);
    return *this;
}

void
HomaTransport::OutboundMessage::copy(const OutboundMessage& other)
{
    this->sxController = other.sxController;
    this->msgId = other.msgId;
    this->msgSize = other.msgSize;
    this->bytesLeft = other.bytesLeft;
    this->nextByteToSend = other.nextByteToSend;
    this->dataBytesInReq = other.dataBytesInReq;
    this->unschedDataBytes = other.unschedDataBytes;
    this->destAddr = other.destAddr;
    this->srcAddr = other.srcAddr;
    this->msgCreationTime = other.msgCreationTime;
}

void
HomaTransport::OutboundMessage::sendRequestAndUnsched()
{
    HomaPkt* reqPkt = new HomaPkt(sxController->transport);
    reqPkt->setPktType(PktType::REQUEST);

    // find reqPkt and unschedPkt priority
    uint16_t reqPrio = sxController->prioResolver->getPrioForPkt(
        sxController->unschedPrioResMode, msgSize, PktType::REQUEST);

    uint16_t unschedPrio = sxController->prioResolver->getPrioForPkt(
        sxController->unschedPrioResMode, msgSize, PktType::UNSCHED_DATA);

    // create request fields and append the data
    RequestFields reqFields;
    reqFields.numReqBytes = this->dataBytesInReq;
    reqFields.totalUnschedBytes = this->dataBytesInReq + this->unschedDataBytes;
    reqFields.msgByteLen = this->bytesLeft;
    reqFields.msgCreationTime = msgCreationTime;
    reqFields.unschedPrio = unschedPrio;
    reqPkt->setReqFields(reqFields);

    // set homa fields
    reqPkt->setDestAddr(this->destAddr);
    reqPkt->setSrcAddr(this->srcAddr);
    reqPkt->setMsgId(this->msgId);
    reqPkt->setPriority(reqPrio);

    // set homa pkt length
    reqPkt->setByteLength(reqPkt->headerSize() + reqFields.numReqBytes);

    sxController->transport->sendPacket(reqPkt);
    EV_INFO << "Send request with msgId " << this->msgId << " from "
            << this->srcAddr.str() << " to " << this->destAddr.str() << endl;

    // update the OutboundMsg if necessary
    this->bytesLeft -= reqFields.numReqBytes;
    this->nextByteToSend += reqFields.numReqBytes;

    // packetize and send unsched data if there is any
    uint32_t unschedBytesToSend = this->unschedDataBytes;
    while (unschedBytesToSend > 0) {
        HomaPkt* unschedPkt = new HomaPkt(sxController->transport);
        unschedPkt->setPktType(PktType::UNSCHED_DATA);
        uint32_t maxDataInPkt = MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE -
                UDP_HEADER_SIZE - unschedPkt->headerSize();

        uint32_t bytesToSend = unschedBytesToSend;
        if (bytesToSend > maxDataInPkt) {
            bytesToSend = maxDataInPkt;
        }
        unschedBytesToSend -= bytesToSend;

        // fill up unschedFields
        UnschedDataFields unschedFields;
        unschedFields.msgByteLen = reqFields.msgByteLen;
        unschedFields.msgCreationTime = msgCreationTime;
        unschedFields.numReqBytes = this->dataBytesInReq;
        unschedFields.reqPrio = reqPrio;
        unschedFields.totalUnschedBytes =
                this->dataBytesInReq + this->unschedDataBytes;
        unschedFields.firstByte = this->nextByteToSend;
        unschedFields.lastByte = this->nextByteToSend + bytesToSend - 1;
        unschedPkt->setUnschedDataFields(unschedFields);

        // set homa fields
        unschedPkt->setDestAddr(this->destAddr);
        unschedPkt->setSrcAddr(this->srcAddr);
        unschedPkt->setMsgId(this->msgId);
        unschedPkt->setPriority(unschedPrio);

        // set homa pkt length
        unschedPkt->setByteLength(unschedPkt->headerSize() + bytesToSend);

        sxController->transport->sendPacket(unschedPkt);
        EV_INFO << "Send unsched pkt with msgId " << this->msgId << " from "
                << this->srcAddr.str() << " to " << this->destAddr.str() << endl;

        // update outboundMsg states and variables
        this->bytesLeft -= bytesToSend;
        this->nextByteToSend = unschedFields.lastByte + 1;
    }
}

int
HomaTransport::OutboundMessage::sendSchedBytes(uint32_t numBytes,
    uint16_t schedPrio)
{
    ASSERT(this->bytesLeft > 0);
    uint32_t bytesToSend = std::min(numBytes, this->bytesLeft);

    // create a data pkt and send out
    HomaPkt* dataPkt = new HomaPkt(sxController->transport);
    dataPkt->setPktType(PktType::SCHED_DATA);
    dataPkt->setSrcAddr(this->srcAddr);
    dataPkt->setDestAddr(this->destAddr);
    dataPkt->setMsgId(this->msgId);
    dataPkt->setPriority(schedPrio);
    SchedDataFields dataFields;
    dataFields.firstByte = this->nextByteToSend;
    dataFields.lastByte = dataFields.firstByte + bytesToSend - 1;
    dataPkt->setSchedDataFields(dataFields);
    dataPkt->setByteLength(bytesToSend + dataPkt->headerSize());
    sxController->transport->sendPacket(dataPkt);

    // update outbound messgae
    this->bytesLeft -= bytesToSend;
    this->nextByteToSend = dataFields.lastByte + 1;

    EV_INFO << "Sent " <<  bytesToSend << " bytes from msgId " << this->msgId
            << " to destination " << this->destAddr.str() << " ("
            << this->bytesLeft << " bytes left.)" << endl;
    return this->bytesLeft;
}


/**
 * HomaTransport::ReceiveScheduler
 */
HomaTransport::ReceiveScheduler::ReceiveScheduler(HomaTransport* transport)
    : transport(transport)
    , grantTimer(NULL)
    , trafficPacer(NULL)
    , unschRateComp(NULL)
    , inboundMsgQueue()
    , incompleteRxMsgs()
    , bytesRecvdPerPrio()
    , scheduledBytesPerPrio()
    , unschedToReceivePerPrio()
    , allBytesRecvd(0)
    , unschedBytesToRecv(0)

{
    trafficPacer = (TrafficPacer*) ::operator new (sizeof(TrafficPacer));
}

void
HomaTransport::ReceiveScheduler::initialize(uint32_t grantMaxBytes,
    uint32_t nicLinkSpeed, uint16_t allPrio, uint16_t schedPrioLevels,
    cMessage* grantTimer, HomaTransport::ReceiveScheduler::QueueType queueType,
    const char* prioAssignMode, PriorityResolver* prioResolver)
{
    this->unschRateComp = new UnschedRateComputer(nicLinkSpeed,
        transport->par("useUnschRateInScheduler").boolValue(), 0.1);

    this->trafficPacer = new(this->trafficPacer) TrafficPacer(prioResolver,
        unschRateComp, nicLinkSpeed, allPrio, schedPrioLevels, grantMaxBytes,
        transport->maxOutstandingRecvBytes, prioAssignMode);
    this->grantTimer = grantTimer;
    inboundMsgQueue.initialize(queueType);
    bytesRecvdPerPrio.resize(allPrio);
    scheduledBytesPerPrio.resize(allPrio);
    unschedToReceivePerPrio.resize(allPrio);
    std::fill(bytesRecvdPerPrio.begin(), bytesRecvdPerPrio.end(), 0);
    std::fill(scheduledBytesPerPrio.begin(), scheduledBytesPerPrio.end(), 0);
    std::fill(unschedToReceivePerPrio.begin(), unschedToReceivePerPrio.end(), 0);
}

HomaTransport::ReceiveScheduler::~ReceiveScheduler()
{
    for (InboundMsgsMap::iterator it = incompleteRxMsgs.begin();
            it != incompleteRxMsgs.end(); ++it) {

        std::list<InboundMessage*> inboundMsgList = it->second;
        for (std::list<InboundMessage*>::iterator iter = inboundMsgList.begin()
                ; iter != inboundMsgList.end(); ++iter) {
            InboundMessage* msgToDelete = *iter;
            delete msgToDelete;
        }
    }
    delete trafficPacer;
    delete unschRateComp;
}

void
HomaTransport::ReceiveScheduler::processReceivedRequest(HomaPkt* rxPkt)
{
    EV_INFO << "Received request with msgId " << rxPkt->getMsgId() << " from "
            << rxPkt->getSrcAddr().str() << " to " << rxPkt->getDestAddr().str()
            << " for " << rxPkt->getReqFields().msgByteLen << " bytes." << endl;

    uint32_t numBytesInReqPkt = rxPkt->getReqFields().numReqBytes;
    unschRateComp->updateUnschRate(simTime(),
        HomaPkt::getBytesOnWire(numBytesInReqPkt, PktType::REQUEST));
    addArrivedBytes(PktType::REQUEST, rxPkt->getPriority(), numBytesInReqPkt);

    // Check if this message already added to incompleteRxMsg. if not create new
    // inbound message in both the msgQueue and incomplete messages.
    InboundMessage* inboundMsg = lookupIncompleteRxMsg(rxPkt);

    if (!inboundMsg) {
        inboundMsg = new InboundMessage(rxPkt, this);
        if (inboundMsg->bytesToGrant > 0) {
            inboundMsgQueue.push(inboundMsg);
        }
        incompleteRxMsgs[rxPkt->getMsgId()].push_front(inboundMsg);

        uint32_t unschedBytesToArrive =
            rxPkt->getReqFields().totalUnschedBytes - numBytesInReqPkt;
        // for any new request, the trailing unsched bytes (IF ANY) that are in
        // the flight to the receiver must be accounted in the future grants we
        // will send as they might interfer with sched data.
        if (unschedBytesToArrive > 0) {
            trafficPacer->unschedPendingBytes(inboundMsg, unschedBytesToArrive,
                PktType::UNSCHED_DATA, rxPkt->getReqFields().unschedPrio);
            transport->emit(totalOutstandingBytesSignal,
                    trafficPacer->getTotalOutstandingBytes());
            addPendingUnschedBytes(PktType::UNSCHED_DATA,
                rxPkt->getReqFields().unschedPrio, unschedBytesToArrive);
        }
        inboundMsg->updatePerPrioStats();
    } else {

        // this request bytes were previously accounted for in the trafficPacer
        // and now we need to return them to the pacer
        transport->emit(totalOutstandingBytesSignal,
                trafficPacer->getTotalOutstandingBytes());
        trafficPacer->bytesArrived(inboundMsg, numBytesInReqPkt,
            PktType::REQUEST, rxPkt->getPriority());
    }
    // If this req. packet also contains data, we need to add the data to the
    // inbound message and update the incompleteRxMsgs.
    if (numBytesInReqPkt > 0) {
        inboundMsg->fillinRxBytes(0, numBytesInReqPkt - 1);
    }

    if (inboundMsg->bytesToReceive <= 0) {
        ASSERT(inboundMsg->bytesToGrant == 0);

        // this message is complete, so send it to the application
        AppMessage* rxMsg = inboundMsg->prepareRxMsgForApp();
        transport->send(rxMsg, "appOut", 0);

        // remove this message from the incompleteRxMsgs
        incompleteRxMsgs[rxPkt->getMsgId()].remove(inboundMsg);
        if (incompleteRxMsgs[rxPkt->getMsgId()].empty()) {
            incompleteRxMsgs.erase(rxPkt->getMsgId());
        }

        delete inboundMsg;
    }
    delete rxPkt;

    // at new pkt arrival, we schedule a new grant
    sendAndScheduleGrant();
}

HomaTransport::InboundMessage*
HomaTransport::ReceiveScheduler::lookupIncompleteRxMsg(HomaPkt* rxPkt)
{
    InboundMsgsMap::iterator rxMsgIdListIter =
            incompleteRxMsgs.find(rxPkt->getMsgId());
    if (rxMsgIdListIter == incompleteRxMsgs.end()) {
        return NULL;
    }
    std::list<InboundMessage*> inboundMsgList = rxMsgIdListIter->second;
    for (std::list<InboundMessage*>::iterator it = inboundMsgList.begin();
            it != inboundMsgList.end(); ++it) {
        InboundMessage* inboundMsg = *it;
        if (rxPkt->getSrcAddr() == inboundMsg->srcAddr) {
            ASSERT(rxPkt->getMsgId() == inboundMsg->msgIdAtSender);
            return inboundMsg;
        }
    }
    return NULL;
}

void
HomaTransport::ReceiveScheduler::processReceivedUnschedData(HomaPkt* rxPkt)
{

    EV_INFO << "Received unsched pkt with msgId " << rxPkt->getMsgId() <<
            " from" << rxPkt->getSrcAddr().str() << " to " <<
            rxPkt->getDestAddr().str() << " for " <<
            rxPkt->getUnschedDataFields().msgByteLen << " bytes." << endl;

    uint32_t pktUnschedBytes = rxPkt->getUnschedDataFields().lastByte -
            rxPkt->getUnschedDataFields().firstByte + 1;
    unschRateComp->updateUnschRate(simTime(),
        HomaPkt::getBytesOnWire(pktUnschedBytes, PktType::UNSCHED_DATA));
    addArrivedBytes(PktType::UNSCHED_DATA, rxPkt->getPriority(),
        pktUnschedBytes);

    if (pktUnschedBytes == 0) {
        cRuntimeError("Unsched pkt with no data byte in it is not allowed.");
    }

    // In the common case, req. pkt is arrived before the unsched packets but we
    // need to account for the rare case that unsched. pkt arrive earlier
    // because of unexpected delay on the req. and so we have to make the new
    // entry in incompleteRxMsgs and inboundMsgQueue.
    InboundMessage* inboundMsg = lookupIncompleteRxMsg(rxPkt);

    if (!inboundMsg) {
        inboundMsg = new InboundMessage(rxPkt, this);
        if (inboundMsg->bytesToGrant > 0) {
            inboundMsgQueue.push(inboundMsg);
        }
        incompleteRxMsgs[rxPkt->getMsgId()].push_front(inboundMsg);

        // There will be a req. packet and possibly other unsched pkts arriving
        // after this unsched. pkt that we need to account for in the
        // trafficPacer.
        uint32_t reqPktDataBytes = rxPkt->getUnschedDataFields().numReqBytes;
        trafficPacer->unschedPendingBytes(inboundMsg, reqPktDataBytes,
            PktType::REQUEST, rxPkt->getUnschedDataFields().reqPrio);
        addPendingUnschedBytes(PktType::REQUEST,
            rxPkt->getUnschedDataFields().reqPrio, reqPktDataBytes);

        uint32_t unschedBytesToArrive =
                rxPkt->getUnschedDataFields().totalUnschedBytes -
                reqPktDataBytes - pktUnschedBytes;
        if (unschedBytesToArrive > 0) {
            trafficPacer->unschedPendingBytes(inboundMsg, unschedBytesToArrive,
                PktType::UNSCHED_DATA, rxPkt->getPriority());
            addPendingUnschedBytes(PktType::UNSCHED_DATA,
                rxPkt->getPriority(), unschedBytesToArrive);
        }
        transport->emit(totalOutstandingBytesSignal,
                trafficPacer->getTotalOutstandingBytes());
        inboundMsg->updatePerPrioStats();
    } else {
        // this is a packet that we expected to receive and have previously
        // accounted for in the trafficPacer. Now we need to return it to the
        // pacer.

        transport->emit(totalOutstandingBytesSignal,
            trafficPacer->getTotalOutstandingBytes());
        trafficPacer->bytesArrived(inboundMsg, pktUnschedBytes,
            PktType::UNSCHED_DATA, rxPkt->getPriority());
    }

    // Append the unsched data to the inboundMsg
    inboundMsg->fillinRxBytes(rxPkt->getUnschedDataFields().firstByte,
            rxPkt->getUnschedDataFields().lastByte);

    if (inboundMsg->bytesToReceive <= 0) {
        ASSERT(inboundMsg->bytesToGrant == 0);

        // this message is complete, so send it to the application
        AppMessage* rxMsg = inboundMsg->prepareRxMsgForApp();
        transport->send(rxMsg, "appOut", 0);

        // remove this message from the incompleteRxMsgs
        incompleteRxMsgs[rxPkt->getMsgId()].remove(inboundMsg);
        if (incompleteRxMsgs[rxPkt->getMsgId()].empty()) {
            incompleteRxMsgs.erase(rxPkt->getMsgId());
        }
        delete inboundMsg;
    }
    delete rxPkt;

    // at new pkt arrival, we schedule a new grant
    sendAndScheduleGrant();
}

void
HomaTransport::ReceiveScheduler::processReceivedSchedData(HomaPkt* rxPkt)
{

    InboundMessage* inboundMsg = lookupIncompleteRxMsg(rxPkt);

    if (!inboundMsg) {
        cRuntimeError("Received unexpected data with msgId %lu from src %s",
                rxPkt->getMsgId(), rxPkt->getSrcAddr().str().c_str());
    }

    incompleteRxMsgs[rxPkt->getMsgId()].remove(inboundMsg);

    uint32_t bytesReceived = (rxPkt->getSchedDataFields().lastByte
            - rxPkt->getSchedDataFields().firstByte + 1);
    addArrivedBytes(PktType::SCHED_DATA, rxPkt->getPriority(), bytesReceived);
    ASSERT(bytesReceived > 0);
    inboundMsg->bytesGrantedInFlight -= bytesReceived;
    inboundMsg->fillinRxBytes(rxPkt->getSchedDataFields().firstByte,
        rxPkt->getSchedDataFields().lastByte);

    // this is a packet we have already committed in the trafficPacer by sending
    // agrant for it. Now the grant for this packet must be returned to the
    // pacer.
    uint32_t totalOnwireBytesReceived = HomaPkt::getBytesOnWire(bytesReceived,
        PktType::SCHED_DATA);
    transport->outstandingGrantBytes -= totalOnwireBytesReceived;
    transport->emit(totalOutstandingBytesSignal, trafficPacer->getTotalOutstandingBytes());
    trafficPacer->bytesArrived(inboundMsg, bytesReceived, PktType::SCHED_DATA,
        rxPkt->getPriority());
    EV_INFO << "Received " << bytesReceived << " bytes from "
            << inboundMsg->srcAddr.str() << " for msgId "
            << rxPkt->getMsgId() << " (" << inboundMsg->bytesToReceive
            << " bytes left to receive)" << endl;

    // If bytesToReceive is zero, this message is complete and must be
    // sent to the application. We should also delete the message from the
    // containers
    if (inboundMsg->bytesToReceive <= 0) {

        // send message to app
        AppMessage* rxMsg = inboundMsg->prepareRxMsgForApp();
        transport->send(rxMsg, "appOut", 0);
        EV_INFO << "Finished receiving msgId " << rxPkt->getMsgId()
                << " with size " << inboundMsg->msgSize
                << " from source address " << inboundMsg->srcAddr
                << endl;

        delete inboundMsg;
        if (incompleteRxMsgs[rxPkt->getMsgId()].empty()) {
            incompleteRxMsgs.erase(rxPkt->getMsgId());
        }

    } else {
        incompleteRxMsgs[rxPkt->getMsgId()].push_front(inboundMsg);
    }
    delete rxPkt;

    // at new pkt arrival, we schedule a new grant
    sendAndScheduleGrant();
}

void
HomaTransport::ReceiveScheduler::sendAndScheduleGrant()
{
    ASSERT(grantTimer->getKind() == HomaTransport::SelfMsgKind::GRANT);
    if (inboundMsgQueue.empty()) {
        return;
    }
    InboundMessage* highPrioMsg = inboundMsgQueue.top();

    simtime_t currentTime = simTime();
    simtime_t nextTimeToGrant = SIMTIME_ZERO;
    HomaPkt* grantPkt =
        trafficPacer->getGrant(currentTime, highPrioMsg, nextTimeToGrant);
    if (grantPkt) {

        // update stats
        uint32_t grantSize = grantPkt->getGrantFields().grantBytes;
        uint16_t prio = grantPkt->getGrantFields().schedPrio;
        addSentGrantBytes(prio, grantSize);

        // update perPrioStats for the highPrioMsg after calling
        // addSentGrantBytes
        highPrioMsg->updatePerPrioStats();
        uint32_t schedBytesOnWire =
            HomaPkt::getBytesOnWire(grantSize, PktType::SCHED_DATA);
        transport->outstandingGrantBytes += schedBytesOnWire;
        transport->emit(outstandingGrantBytesSignal,
            transport->outstandingGrantBytes);

        // send the grant out
        transport->sendPacket(grantPkt);

        // set the grantTimer for the next grant
        transport->scheduleAt(nextTimeToGrant, grantTimer);

        // Pop the highPrioMsg from msgQueue and push it back if it still needs
        // grant. This is necessary for round robin scheduling and doesn't
        // make any difference for SRPT scheduler.
        inboundMsgQueue.pop();
        if (highPrioMsg->bytesToGrant > 0) {
            inboundMsgQueue.push(highPrioMsg);
        }

        EV_INFO << " Sent a grant, among " << inboundMsgQueue.size()
                << " possible choices, for msgId " << highPrioMsg->msgIdAtSender
                << " at host " << highPrioMsg->srcAddr.str() << " for "
                << grantSize << " Bytes." << "(" << highPrioMsg->bytesToGrant
                << " Bytes left to grant.)" << endl;
    }
}

HomaTransport::ReceiveScheduler::UnschedRateComputer::UnschedRateComputer(
        uint32_t nicLinkSpeed, bool computeAvgUnschRate,
        double minAvgTimeWindow)
    : computeAvgUnschRate(computeAvgUnschRate)
    , bytesRecvTime()
    , sumBytes(0)
    , minAvgTimeWindow(minAvgTimeWindow)
    , nicLinkSpeed(nicLinkSpeed)
{

}


/**
 * Returns the fraction of bw used by the unsched (eg. average unsched rate
 * divided by nic link speed.)
 */
double
HomaTransport::ReceiveScheduler::UnschedRateComputer::getAvgUnschRate(
    simtime_t currentTime)
{
    if (!computeAvgUnschRate) {
        return 0;
    }

    if (bytesRecvTime.size() == 0) {
        return 0.0;
    }
    double timeDuration = currentTime.dbl() - bytesRecvTime.front().second;
    if (timeDuration <= minAvgTimeWindow/100)  {
        return 0.0;
    }
    double avgUnschFractionRate =
        ((double)sumBytes*8/timeDuration)*1e-9/nicLinkSpeed;
    ASSERT(avgUnschFractionRate < 1.0);
    return avgUnschFractionRate;
}

void
HomaTransport::ReceiveScheduler::UnschedRateComputer::updateUnschRate(
    simtime_t arrivalTime, uint32_t bytesRecvd)
{
    if (!computeAvgUnschRate) {
        return;
    }

    bytesRecvTime.push_back(std::make_pair(bytesRecvd, arrivalTime.dbl()));
    sumBytes += bytesRecvd;
    for (auto bytesTimePair = bytesRecvTime.begin();
            bytesTimePair != bytesRecvTime.end();)
    {
        double deltaTime = arrivalTime.dbl() - bytesTimePair->second;
        if (deltaTime <= minAvgTimeWindow) {
            return;
        }

        if (deltaTime > minAvgTimeWindow) {
            sumBytes -= bytesTimePair->first;
            bytesTimePair = bytesRecvTime.erase(bytesTimePair);
        } else {
            ++bytesTimePair;
        }
    }
}

void
HomaTransport::ReceiveScheduler::addArrivedBytes(PktType pktType, uint16_t prio,
    uint32_t dataBytesInPkt)
{
    uint32_t arrivedBytesOnWire = HomaPkt::getBytesOnWire(dataBytesInPkt, pktType);
    allBytesRecvd += arrivedBytesOnWire;
    bytesRecvdPerPrio.at(prio) += arrivedBytesOnWire;
}

void
HomaTransport::ReceiveScheduler::addSentGrantBytes(uint16_t prio,
    uint32_t grantedBytes)
{
    uint32_t schedBytesOnWire =
        HomaPkt::getBytesOnWire(grantedBytes, PktType::SCHED_DATA);
    scheduledBytesPerPrio.at(prio) += schedBytesOnWire;
}

void
HomaTransport::ReceiveScheduler::addPendingUnschedBytes(PktType pktType,
    uint16_t prio, uint32_t bytesToArrive)
{
    uint32_t unschedBytesOnWire =
        HomaPkt::getBytesOnWire(bytesToArrive, pktType);
    unschedToReceivePerPrio.at(prio) += unschedBytesOnWire;
}

HomaTransport::ReceiveScheduler::InboundMsgQueue::InboundMsgQueue()
    : prioQueue()
    , fifoQueue()
    , queueType()
{}

void
HomaTransport::ReceiveScheduler::InboundMsgQueue::initialize(
        HomaTransport::ReceiveScheduler::QueueType queueType)
{
    if (queueType >= INVALID_TYPE || queueType < 0) {
        cRuntimeError("InboundMsgQueue was constructed with an invalid type.");
    }
    this->queueType = queueType;
}

void
HomaTransport::ReceiveScheduler::InboundMsgQueue::push(
        HomaTransport::InboundMessage* inboundMsg)
{
    if (queueType == QueueType::PRIO_QUEUE) {
        prioQueue.push(inboundMsg);
    } else if (queueType == QueueType::FIFO_QUEUE) {
        fifoQueue.push(inboundMsg);
    }
}

HomaTransport::InboundMessage*
HomaTransport::ReceiveScheduler::InboundMsgQueue::top()
{
    if (queueType == QueueType::PRIO_QUEUE) {
        return prioQueue.top();
    } else if (queueType == QueueType::FIFO_QUEUE) {
        return fifoQueue.front();
    }
    return NULL;
}

void
HomaTransport::ReceiveScheduler::InboundMsgQueue::pop()
{
    if (queueType == QueueType::PRIO_QUEUE) {
        prioQueue.pop();
    } else if (queueType == QueueType::FIFO_QUEUE) {
        fifoQueue.pop();
    }
}

bool
HomaTransport::ReceiveScheduler::InboundMsgQueue::empty()
{
    if (queueType == QueueType::PRIO_QUEUE) {
        return prioQueue.empty();
    } else if (queueType == QueueType::FIFO_QUEUE) {
        return fifoQueue.empty();
    }
    return false;
}

size_t
HomaTransport::ReceiveScheduler::InboundMsgQueue::size()
{
    if (queueType == QueueType::PRIO_QUEUE) {
        return prioQueue.size();
    } else if (queueType == QueueType::FIFO_QUEUE) {
        return fifoQueue.size();
    }
    return 0;
}


/**
 * HomaTransport::InboundMessage
 */
HomaTransport::InboundMessage::InboundMessage()
    : rxScheduler(NULL)
    , srcAddr()
    , destAddr()
    , msgIdAtSender(0)
    , bytesToGrant(0)
    , bytesGrantedInFlight(0)
    , bytesToReceive(0)
    , msgSize(0)
    , bytesInReq(0)
    , totalUnschedBytes(0)
    , msgCreationTime(SIMTIME_ZERO)
    , reqArrivalTime(simTime())
    , lastGrantTime(simTime())
    , highPrioSchedDelayBytes(0)
    , bytesRecvdPerPrio()
    , scheduledBytesPerPrio()
    , unschedToReceivePerPrio()
{}

HomaTransport::InboundMessage::~InboundMessage()
{}

HomaTransport::InboundMessage::InboundMessage(const InboundMessage& other)
{
    copy(other);
}

HomaTransport::InboundMessage::InboundMessage(HomaPkt* rxPkt,
        ReceiveScheduler* rxScheduler)
    : rxScheduler(rxScheduler)
    , srcAddr(rxPkt->getSrcAddr())
    , destAddr(rxPkt->getDestAddr())
    , msgIdAtSender(rxPkt->getMsgId())
    , bytesToGrant(0)
    , bytesGrantedInFlight(0)
    , bytesToReceive(0)
    , msgSize(0)
    , bytesInReq(0)
    , totalUnschedBytes(0)
    , msgCreationTime(SIMTIME_ZERO)
    , reqArrivalTime(simTime())
    , lastGrantTime(simTime())
    , highPrioSchedDelayBytes(0)
    , bytesRecvdPerPrio()
    , scheduledBytesPerPrio()
    , unschedToReceivePerPrio()
{
    switch (rxPkt->getPktType()) {
        case PktType::REQUEST:
            bytesToGrant = rxPkt->getReqFields().msgByteLen -
                    rxPkt->getReqFields().totalUnschedBytes;
            bytesToReceive = rxPkt->getReqFields().msgByteLen;
            msgSize = rxPkt->getReqFields().msgByteLen;
            bytesInReq = rxPkt->getReqFields().numReqBytes;
            totalUnschedBytes = rxPkt->getReqFields().totalUnschedBytes;
            msgCreationTime = rxPkt->getReqFields().msgCreationTime;
            break;
        case PktType::UNSCHED_DATA:
            bytesToGrant = rxPkt->getUnschedDataFields().msgByteLen -
                    rxPkt->getUnschedDataFields().totalUnschedBytes;
            bytesToReceive = rxPkt->getUnschedDataFields().msgByteLen;
            msgSize = rxPkt->getUnschedDataFields().msgByteLen;
            bytesInReq = rxPkt->getUnschedDataFields().numReqBytes;
            totalUnschedBytes = rxPkt->getUnschedDataFields().totalUnschedBytes;
            msgCreationTime = rxPkt->getUnschedDataFields().msgCreationTime;
            break;
        default:
            cRuntimeError("Can't create inbound message "
                    "from received packet type: %d.", rxPkt->getPktType());
    }
}

void
HomaTransport::InboundMessage::fillinRxBytes(uint32_t byteStart,
        uint32_t byteEnd)
{
    uint32_t bytesReceived = byteEnd - byteStart + 1;
    bytesToReceive -= bytesReceived;
    EV_INFO << "Received " << bytesReceived << " bytes from " << srcAddr.str()
            << " for msgId " << msgIdAtSender << " (" << bytesToReceive <<
            " bytes left to receive)" << endl;

}


HomaPkt*
HomaTransport::InboundMessage::prepareGrant(uint32_t grantSize, uint16_t schedPrio)
{

    // prepare a grant
    HomaPkt* grantPkt = new HomaPkt(rxScheduler->transport);
    grantPkt->setPktType(PktType::GRANT);
    GrantFields grantFields;
    grantFields.grantBytes = grantSize;
    grantFields.schedPrio = schedPrio;
    grantPkt->setGrantFields(grantFields);
    grantPkt->setDestAddr(this->srcAddr);
    grantPkt->setSrcAddr(this->destAddr);
    grantPkt->setMsgId(this->msgIdAtSender);
    grantPkt->setByteLength(grantPkt->headerSize());
    grantPkt->setPriority(0);

    int allHighPrioInflightBytes = 0;
    // Calculate the high priority sched. delay
    if (bytesToGrant == msgSize-totalUnschedBytes) {

        // This is the first grant we're sending for this message. If the
        // outstanding bytes at req. arrival has been more than
        // maxOutstandingRecvBytes, then the receiver obviously has delayed the
        // grant for this message. So we need to account how much of that delay
        // has been because of high prio messages.
        for (size_t i = 0; i <= schedPrio; i++) {
            allHighPrioInflightBytes += sumInflightUnschedPerPrio[i];
            allHighPrioInflightBytes += sumInflightSchedPerPrio[i];
        }

        if (allHighPrioInflightBytes >
                rxScheduler->transport->maxOutstandingRecvBytes) {
            highPrioSchedDelayBytes += (allHighPrioInflightBytes -
                rxScheduler->transport->maxOutstandingRecvBytes);
        }
    }

    for (size_t i = 0; i <= schedPrio; i++) {
        allHighPrioInflightBytes += (rxScheduler->scheduledBytesPerPrio[i] -
            this->scheduledBytesPerPrio[i]);
        allHighPrioInflightBytes += (rxScheduler->unschedToReceivePerPrio[i] -
            this->unschedToReceivePerPrio[i]);
    }

    // update internal structure
    this->bytesToGrant -= grantSize;
    this->bytesGrantedInFlight += grantSize;
    this->lastGrantTime = simTime();

    return grantPkt;
}

AppMessage*
HomaTransport::InboundMessage::prepareRxMsgForApp()
{
    AppMessage* rxMsg = new AppMessage();
    rxMsg->setDestAddr(this->destAddr);
    rxMsg->setSrcAddr(this->srcAddr);
    rxMsg->setByteLength(this->msgSize);
    rxMsg->setMsgCreationTime(this->msgCreationTime);

    // calculate scheduling delay
    if (totalUnschedBytes >= msgSize) {
        // no grant was expected for this message
        ASSERT(reqArrivalTime == lastGrantTime && bytesToGrant == 0 &&
            bytesToReceive == 0);
        rxMsg->setTransportSchedDelay(SIMTIME_ZERO);
    } else {
        ASSERT(reqArrivalTime <= lastGrantTime && bytesToGrant == 0 &&
            bytesToReceive == 0);
        simtime_t totalSchedTime = lastGrantTime - reqArrivalTime;
        simtime_t minPossibleSchedTime;
        uint32_t bytesGranted = msgSize - totalUnschedBytes;
        uint32_t bytesGrantedOnWire = HomaPkt::getBytesOnWire(bytesGranted,
            PktType::SCHED_DATA);
        uint32_t maxPktSizeOnWire = ETHERNET_PREAMBLE_SIZE + ETHERNET_HDR_SIZE +
            MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_CRC_SIZE + INTER_PKT_GAP;

        if (bytesGrantedOnWire <= maxPktSizeOnWire) {
            minPossibleSchedTime = SIMTIME_ZERO;
        } else {
            uint32_t lastPktSize = bytesGrantedOnWire % maxPktSizeOnWire;
            if (lastPktSize == 0) {
                lastPktSize = maxPktSizeOnWire;
            }
            minPossibleSchedTime =
                SimTime(1e-9 * (bytesGrantedOnWire-lastPktSize) * 8.0 /
                rxScheduler->transport->nicLinkSpeed) ;
        }
        simtime_t schedDelay = totalSchedTime - minPossibleSchedTime;
        rxMsg->setTransportSchedDelay(schedDelay);
        rxMsg->setTransportSchedPreemptionLag(schedDelay.dbl() -
            highPrioSchedDelayBytes*8*1e-9/rxScheduler->transport->nicLinkSpeed);
    }
    return rxMsg;
}

void
HomaTransport::InboundMessage::updatePerPrioStats()
{
    this->bytesRecvdPerPrio = rxScheduler->bytesRecvdPerPrio;
    this->scheduledBytesPerPrio = rxScheduler->scheduledBytesPerPrio;
    this->unschedToReceivePerPrio = rxScheduler->unschedToReceivePerPrio;
    this->sumInflightUnschedPerPrio =
        rxScheduler->trafficPacer->getSumInflightUnschedPerPrio();
    this->sumInflightSchedPerPrio =
        rxScheduler->trafficPacer->getSumInflightSchedPerPrio();
}

uint32_t
HomaTransport::InboundMessage::schedBytesInFlight()
{
    return bytesGrantedInFlight;
}

uint32_t
HomaTransport::InboundMessage::unschedBytesInFlight()
{
    ASSERT(bytesToReceive >= bytesToGrant + bytesGrantedInFlight);
    return bytesToReceive - bytesToGrant - schedBytesInFlight();
}

void
HomaTransport::InboundMessage::copy(const InboundMessage& other)
{
    this->rxScheduler = other.rxScheduler;
    this->srcAddr = other.srcAddr;
    this->destAddr = other.destAddr;
    this->msgIdAtSender = other.msgIdAtSender;
    this->bytesToGrant = other.bytesToGrant;
    this->bytesToReceive = other.bytesToReceive;
    this->msgSize = other.msgSize;
    this->bytesInReq = other.bytesInReq;
    this->totalUnschedBytes = other.totalUnschedBytes;
    this->msgCreationTime = other.msgCreationTime;
}
