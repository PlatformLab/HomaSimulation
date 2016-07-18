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
#include "transport/PriorityResolver.h"
#include "HomaTransport.h"

Define_Module(HomaTransport);

/**
 * Registering all statistics collection signals.
 */
simsignal_t HomaTransport::msgsLeftToSendSignal =
        registerSignal("msgsLeftToSend");
simsignal_t HomaTransport::bytesLeftToSendSignal =
        registerSignal("bytesLeftToSend");
simsignal_t HomaTransport::bytesNeedGrantSignal =
        registerSignal("bytesNeedGrant");
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
    , socket()
    , localAddr()
    , homaConfig(NULL)
    , prioResolver(NULL)
    , distEstimator(NULL)
    , grantTimer(NULL)
    , sendTimer(NULL)
{}

/**
 * Destructor of HomaTransport.
 */
HomaTransport::~HomaTransport()
{
    delete prioResolver;
    delete distEstimator;
    delete homaConfig;
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
    homaConfig = new HomaConfigDepot(this);
    HomaPkt dataPkt = HomaPkt();
    dataPkt.setPktType(PktType::SCHED_DATA);
    uint32_t maxDataBytes = MAX_ETHERNET_PAYLOAD_BYTES -
        IP_HEADER_SIZE - UDP_HEADER_SIZE - dataPkt.headerSize();
    if (homaConfig->grantMaxBytes > maxDataBytes) {
        homaConfig->grantMaxBytes = maxDataBytes;
    }
    grantTimer = new cMessage("GrantTimer");
    grantTimer->setKind(SelfMsgKind::START);
    sendTimer = new cMessage("SendTimer");
    sendTimer->setKind(SelfMsgKind::START);
    registerTemplatedStats(homaConfig->allPrio);

    distEstimator = new WorkloadEstimator(homaConfig);
    prioResolver = new PriorityResolver(homaConfig, distEstimator);
    prioResolver->recomputeCbf(homaConfig->cbfCapMsgSize);
    rxScheduler.initialize(homaConfig, grantTimer, prioResolver);
    sxController.initSendController(homaConfig, prioResolver);
    scheduleAt(simTime(), grantTimer);
    outstandingGrantBytes = 0;
}

void
HomaTransport::processStart()
{
    socket.setOutputGate(gate("udpOut"));
    socket.bind(homaConfig->localPort);
    grantTimer->setKind(SelfMsgKind::GRANT);
    sendTimer->setKind(SelfMsgKind::SEND);
}

void
HomaTransport::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        switch (msg->getKind()) {
            case SelfMsgKind::START:
                processStart();
                break;
            case SelfMsgKind::GRANT:
                ASSERT(msg == grantTimer);
                rxScheduler.sendAndScheduleGrant();
                break;
            case SelfMsgKind::SEND:
                ASSERT(msg == sendTimer);
                sxController.sendOrQueue(msg);
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
HomaTransport::sendPktAndScheduleNext(HomaPkt* sxPkt)
{
    uint32_t pktLenOnWire =
        HomaPkt::getBytesOnWire(sxPkt->getDataBytes(), (PktType)sxPkt->getPktType());
    simtime_t currentTime = simTime();
    simtime_t nextSendTime = SimTime(1e-9 * (pktLenOnWire * 8.0 /
        homaConfig->nicLinkSpeed)) + currentTime;
    socket.sendTo(sxPkt, sxPkt->getDestAddr(), homaConfig->destPort);
    scheduleAt(nextSendTime, sendTimer);
}

void
HomaTransport::finish()
{
    cancelAndDelete(grantTimer);
    cancelAndDelete(sendTimer);
}


/**
 * HomaTransport::SendController
 */
HomaTransport::SendController::SendController(HomaTransport* transport)
    : bytesLeftToSend(0)
    , bytesNeedGrant(0)
    , msgId(0)
    , outboundMsgMap()
    , unschedByteAllocator(NULL)
    , prioResolver(NULL)
    , outbndMsgSet()
    , transport(transport)
    , homaConfig(NULL)

{}

void
HomaTransport::SendController::initSendController(HomaConfigDepot* homaConfig,
    PriorityResolver* prioResolver)
{
    this->homaConfig = homaConfig;
    this->prioResolver = prioResolver;
    bytesLeftToSend = 0;
    bytesNeedGrant = 0;
    std::random_device rd;
    std::mt19937_64 merceneRand(rd());
    std::uniform_int_distribution<uint64_t> dist(0, UINTMAX_MAX);
    msgId = dist(merceneRand);
    unschedByteAllocator = new UnschedByteAllocator(homaConfig);
}

HomaTransport::SendController::~SendController()
{
    delete unschedByteAllocator;
    while (!outGrantQueue.empty()) {
        HomaPkt* head = outGrantQueue.top();
        outGrantQueue.pop();
        delete head;
    }

}

void
HomaTransport::SendController::processSendMsgFromApp(AppMessage* sendMsg)
{
    transport->emit(msgsLeftToSendSignal, outboundMsgMap.size());
    transport->emit(bytesLeftToSendSignal, bytesLeftToSend);
    transport->emit(bytesNeedGrantSignal, bytesNeedGrant);
    uint32_t destAddr =
            sendMsg->getDestAddr().toIPv4().getInt();
    uint32_t msgSize = sendMsg->getByteLength();
    std::vector<uint16_t> reqUnschedDataVec =
        unschedByteAllocator->getReqUnschedDataPkts(destAddr, msgSize);

    outboundMsgMap.emplace(std::piecewise_construct,
            std::forward_as_tuple(msgId),
            std::forward_as_tuple(sendMsg, this, msgId, reqUnschedDataVec));
    OutboundMessage* outboundMsg = &(outboundMsgMap.at(msgId));
    outboundMsg->prepareRequestAndUnsched();
    auto insResult = outbndMsgSet.insert(outboundMsg);
    ASSERT(insResult.second); // check insertion took place
    bytesLeftToSend += outboundMsg->getBytesLeft();
    bytesNeedGrant += outboundMsg->bytesToSched;
    msgId++;
    delete sendMsg;
    sendOrQueue();
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
    size_t numRemoved = outbndMsgSet.erase(outboundMsg);
    ASSERT(numRemoved <= 1); // At most one item removed

    int bytesToSchedOld = outboundMsg->bytesToSched;
    int bytesToSchedNew = outboundMsg->prepareSchedPkt(
        rxPkt->getGrantFields().grantBytes, rxPkt->getGrantFields().schedPrio);
    int bytesSent = bytesToSchedOld - bytesToSchedNew;
    bytesNeedGrant -= bytesSent;
    ASSERT(bytesSent > 0 && bytesLeftToSend >= 0 && bytesNeedGrant >= 0);
    auto insResult = outbndMsgSet.insert(outboundMsg);
    ASSERT(insResult.second); // check insertion took place
    delete rxPkt;
    sendOrQueue();
}

void
HomaTransport::SendController::sendOrQueue(cMessage* msg)
{
    HomaPkt* sxPkt;
    if (msg == transport->sendTimer) {
        ASSERT(msg->getKind() == SelfMsgKind::SEND);
        if (!outGrantQueue.empty()) {
            sxPkt = outGrantQueue.top();
            outGrantQueue.pop();
            transport->sendPktAndScheduleNext(sxPkt);
            return;
        }

        if (!outbndMsgSet.empty()) {
            OutboundMessage* highPrioMsg = *(outbndMsgSet.begin());
            size_t numRemoved =  outbndMsgSet.erase(highPrioMsg);
            ASSERT(numRemoved == 1); // check only one msg is removed
            bool hasMoreReadyPkt = highPrioMsg->getTransmitReadyPkt(&sxPkt);
            bytesLeftToSend -= sxPkt->getDataBytes();
            transport->sendPktAndScheduleNext(sxPkt);
            if (highPrioMsg->getBytesLeft() <= 0) {
                ASSERT(!hasMoreReadyPkt);
                outboundMsgMap.erase(highPrioMsg->getMsgId());
                return;
            }

            if (hasMoreReadyPkt) {
                auto insResult = outbndMsgSet.insert(highPrioMsg);
                ASSERT(insResult.second); // check insertion took place
                return;
            }
        }
        return;
    }
    sxPkt = dynamic_cast<HomaPkt*>(msg);
    if (sxPkt) {
        ASSERT(sxPkt->getPktType() == PktType::GRANT);
        if (transport->sendTimer->isScheduled()) {
            outGrantQueue.push(sxPkt);
            return;
        } else {
            ASSERT(outGrantQueue.empty());
            transport->sendPktAndScheduleNext(sxPkt);
            return;
        }
    }

    ASSERT(!msg);
    if (transport->sendTimer->isScheduled()) {
        return;
    }

    ASSERT(outGrantQueue.empty() && outbndMsgSet.size() == 1);
    OutboundMessage* highPrioMsg = *(outbndMsgSet.begin());
    size_t numRemoved =  outbndMsgSet.erase(highPrioMsg);
    ASSERT(numRemoved == 1); // check the msg is removed
    bool hasMoreReadyPkt = highPrioMsg->getTransmitReadyPkt(&sxPkt);
    bytesLeftToSend -= sxPkt->getDataBytes();
    transport->sendPktAndScheduleNext(sxPkt);
    if (highPrioMsg->getBytesLeft() <= 0) {
        ASSERT(!hasMoreReadyPkt);
        outboundMsgMap.erase(highPrioMsg->getMsgId());
        return;
    }

    if (hasMoreReadyPkt) {
        auto insResult = outbndMsgSet.insert(highPrioMsg);
        ASSERT(insResult.second); // check insertion took place
        return;
    }
}

/**
 * Predicate operator for comparing OutboundMessages. This function
 * by design return strict ordering on messages meaning that no two
 * distinct message ever compare equal. (ie result of !(a,b)&&!(b,a)
 * is always false)
 *
 * \param msg1
 *      first queue of outbound pkts for comparison
 * \param msg2
 *      second queue of outbound pkts for comparison
 * \return
 *      true if msg1 is compared greater than q2 (ie. pkts of msg1
 *      are in lower priority for transmission than pkts2 of msg2)
 */
bool
HomaTransport::SendController::OutbndMsgSorter::operator()(
        OutboundMessage* msg1, OutboundMessage* msg2) {

    switch (msg1->homaConfig->getSenderScheme()) {
        case HomaConfigDepot::SenderScheme::OBSERVE_PKT_PRIOS: {
            auto &q1 = msg1->getTxPktQueue();
            auto &q2 = msg2->getTxPktQueue();
            HomaPkt* pkt1 = q1.top();
            HomaPkt* pkt2 = q2.top();
            return (pkt1->getPriority() < pkt2->getPriority()) ||
                (pkt1->getPriority() == pkt2->getPriority() &&
                pkt1->getCreationTime() < pkt2->getCreationTime()) ||
                (pkt1->getPriority() == pkt2->getPriority() &&
                pkt1->getCreationTime() == pkt2->getCreationTime() &&
                msg1->getMsgCreationTime() < 
                msg2->getMsgCreationTime()) ||
                (pkt1->getPriority() == pkt2->getPriority() &&
                pkt1->getCreationTime() == pkt2->getCreationTime() &&
                msg1->getMsgCreationTime() == 
                msg2->getMsgCreationTime() &&
                msg1->getMsgId() < msg2->getMsgId());
        }
        case HomaConfigDepot::SenderScheme::SRBF: 
            return msg1->getBytesLeft() < msg2->getBytesLeft() ||
                (msg1->getBytesLeft() == msg2->getBytesLeft() &&
                msg1->getMsgCreationTime() < msg2->getMsgCreationTime()) ||
                (msg1->getBytesLeft() == msg2->getBytesLeft() &&
                msg1->getMsgCreationTime() == msg2->getMsgCreationTime() &&
                msg1->getMsgId() < msg2->getMsgId());

        default:
            throw cRuntimeError("Undefined SenderScheme parameter");
    }
}


/**
 * HomaTransport::OutboundMessage
 */
HomaTransport::OutboundMessage::OutboundMessage(AppMessage* outMsg,
        SendController* sxController, uint64_t msgId,
        std::vector<uint16_t> reqUnschedDataVec)
    : msgId(msgId)
    , msgSize(outMsg->getByteLength())
    , bytesToSched(outMsg->getByteLength())
    , nextByteToSched(0)
    , bytesLeft(outMsg->getByteLength())
    , reqUnschedDataVec(reqUnschedDataVec)
    , destAddr(outMsg->getDestAddr())
    , srcAddr(outMsg->getSrcAddr())
    , msgCreationTime(outMsg->getMsgCreationTime())
    , sxController(sxController)
    , homaConfig(sxController->homaConfig)
{}

HomaTransport::OutboundMessage::OutboundMessage()
    : msgId(0)
    , msgSize(0)
    , bytesToSched(0)
    , nextByteToSched(0)
    , bytesLeft(0)
    , reqUnschedDataVec({0})
    , destAddr()
    , srcAddr()
    , msgCreationTime(SIMTIME_ZERO)
    , sxController(NULL)
    , homaConfig(sxController->homaConfig)

{}

HomaTransport::OutboundMessage::~OutboundMessage()
{
    while (!txPkts.empty()) {
        HomaPkt* head = txPkts.top();
        txPkts.pop();
        delete head;
    }
}

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
    this->homaConfig = other.homaConfig;
    this->msgId = other.msgId;
    this->msgSize = other.msgSize;
    this->bytesToSched = other.bytesToSched;
    this->nextByteToSched = other.nextByteToSched;
    this->bytesLeft = other.bytesLeft;
    this->reqUnschedDataVec = other.reqUnschedDataVec;
    this->destAddr = other.destAddr;
    this->srcAddr = other.srcAddr;
    this->msgCreationTime = other.msgCreationTime;
}

void
HomaTransport::OutboundMessage::prepareRequestAndUnsched()
{
    PriorityResolver::PrioResolutionMode unschedPrioResMode =
        sxController->prioResolver->strPrioModeToInt(
        homaConfig->unschedPrioResolutionMode);
    std::vector<uint16_t> unschedPrioVec =
        sxController->prioResolver->getUnschedPktsPrio(
        unschedPrioResMode, this);

    uint32_t totalUnschedBytes = 0;
    std::vector<uint32_t> prioUnschedBytes = {};
    uint32_t prio = UINT16_MAX;
    for (size_t i = 0; i < this->reqUnschedDataVec.size(); i++) {
        if (unschedPrioVec[i] != prio) {
            prio = unschedPrioVec[i];
            prioUnschedBytes.push_back(prio);
            prioUnschedBytes.push_back(reqUnschedDataVec[i]);
        } else {
            prioUnschedBytes.back() += reqUnschedDataVec[i];
        }
        totalUnschedBytes += reqUnschedDataVec[i];
    }

    //First pkt, always request
    PktType pktType = PktType::REQUEST;
    size_t i = 0;
    do {
        HomaPkt* unschedPkt = new HomaPkt(sxController->transport);
        unschedPkt->setPktType(pktType);

        // set homa fields
        unschedPkt->setDestAddr(this->destAddr);
        unschedPkt->setSrcAddr(this->srcAddr);
        unschedPkt->setMsgId(this->msgId);
        unschedPkt->setPriority(unschedPrioVec[i]);

        // fill up unschedFields
        UnschedFields unschedFields;
        unschedFields.msgByteLen = msgSize;
        unschedFields.msgCreationTime = msgCreationTime;
        unschedFields.totalUnschedBytes = totalUnschedBytes;
        unschedFields.firstByte = this->nextByteToSched;
        unschedFields.lastByte =
            this->nextByteToSched + this->reqUnschedDataVec[i] - 1;
        unschedFields.prioUnschedBytes = prioUnschedBytes;
        for (size_t j = 0;
                j < unschedFields.prioUnschedBytes.size(); j += 2) {
            if (unschedFields.prioUnschedBytes[j] == unschedPkt->getPriority()) {
                // find the two elements in this prioUnschedBytes vec that
                // corresponds to the priority of this packet and subtract
                // the bytes in this packet from prioUnschedBytes.
                unschedFields.prioUnschedBytes[j+1] -= this->reqUnschedDataVec[i];
                if (unschedFields.prioUnschedBytes[j+1] <= 0) {
                    // Delete this element if unsched bytes on this prio is
                    // zero
                    unschedFields.prioUnschedBytes.erase(
                        unschedFields.prioUnschedBytes.begin()+j,
                        unschedFields.prioUnschedBytes.begin()+j+2);
                }
                break;
            }
        }
        unschedPkt->setUnschedFields(unschedFields);
        unschedPkt->setByteLength(unschedPkt->headerSize() +
            unschedPkt->getDataBytes());
        txPkts.push(unschedPkt);
        EV_INFO << "Unsched pkt with msgId " << this->msgId << " ready for"
            " transmit from " << this->srcAddr.str() << " to " <<
            this->destAddr.str() << endl;

        // update the OutboundMsg for the bytes sent in this iteration of loop
        this->bytesToSched -= unschedPkt->getDataBytes();
        this->nextByteToSched += unschedPkt->getDataBytes();

        // all packet except the first one are UNSCHED
        pktType = PktType::UNSCHED_DATA;
        i++;
    } while (i < reqUnschedDataVec.size());
}

uint32_t
HomaTransport::OutboundMessage::prepareSchedPkt(uint32_t numBytes,
    uint16_t schedPrio)
{
    ASSERT(this->bytesToSched > 0);
    uint32_t bytesToSend = std::min(numBytes, this->bytesToSched);

    // create a data pkt and push it txPkts queue for 
    HomaPkt* dataPkt = new HomaPkt(sxController->transport);
    dataPkt->setPktType(PktType::SCHED_DATA);
    dataPkt->setSrcAddr(this->srcAddr);
    dataPkt->setDestAddr(this->destAddr);
    dataPkt->setMsgId(this->msgId);
    dataPkt->setPriority(schedPrio);
    SchedDataFields dataFields;
    dataFields.firstByte = this->nextByteToSched;
    dataFields.lastByte = dataFields.firstByte + bytesToSend - 1;
    dataPkt->setSchedDataFields(dataFields);
    dataPkt->setByteLength(bytesToSend + dataPkt->headerSize());
    txPkts.push(dataPkt);

    // update outbound messgae
    this->bytesToSched -= bytesToSend;
    this->nextByteToSched = dataFields.lastByte + 1;

    EV_INFO << "Prepared " <<  bytesToSend << " bytes for transmission from"
            << " msgId " << this->msgId << " to destination " <<
            this->destAddr.str() << " (" << this->bytesToSched <<
            " bytes left.)" << endl;
    return this->bytesToSched;
}

/**
 * Among all transmission reay packets for this message, this function retrieve
 * highest priority packet from sender's perspective for this message.
 *
 * \return outPkt
 *      The packet to be transmitted for this message
 * \return 
 *      True if if the returned pkt is not the last ready for transmission
 *      packet for this msg and this message hase more pkts queue up and ready
 *      for transmission.
 */
bool
HomaTransport::OutboundMessage::getTransmitReadyPkt(HomaPkt** outPkt)
{
    ASSERT(!txPkts.empty());
    HomaPkt* head = txPkts.top();
    txPkts.pop();
    bytesLeft -= head->getDataBytes();
    *outPkt = head; 
    return !txPkts.empty();
}

/**
 * A utility predicate for comparing HomaPkt instances
 * based on pkt information and senderScheme param.
 *
 * \param pkt1
 *      first pkt for comparison
 * \param pkt2
 *      second pkt for comparison
 * \return
 *      true if pkt1 is compared greater than pkt2.
 */
bool
HomaTransport::OutboundMessage::OutbndPktSorter::operator()(const HomaPkt* pkt1,
    const HomaPkt* pkt2)
{
    HomaConfigDepot::SenderScheme txScheme =
        pkt1->ownerTransport->homaConfig->getSenderScheme();
    switch (txScheme) {
        case HomaConfigDepot::SenderScheme::OBSERVE_PKT_PRIOS:
        case HomaConfigDepot::SenderScheme::SRBF:
            return (pkt1->getPriority() > pkt2->getPriority()) ||
                (pkt1->getPriority() == pkt2->getPriority() &&
                pkt1->getCreationTime() > pkt2->getCreationTime()) ||
                (pkt1->getPriority() == pkt2->getPriority() &&
                pkt1->getCreationTime() == pkt2->getCreationTime() &&
                pkt1->getUnschedFields().firstByte >
                pkt2->getUnschedFields().firstByte);
        default:
                throw cRuntimeError("Undefined SenderScheme parameter");
    }
}


/**
 * HomaTransport::ReceiveScheduler
 */
HomaTransport::ReceiveScheduler::ReceiveScheduler(HomaTransport* transport)
    : transport(transport)
    , homaConfig(NULL)
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
HomaTransport::ReceiveScheduler::initialize(HomaConfigDepot* homaConfig,
    cMessage* grantTimer, PriorityResolver* prioResolver)
{
    this->homaConfig = transport->homaConfig;
    HomaTransport::ReceiveScheduler::QueueType queueType;
    if (homaConfig->isRoundRobinScheduler) {
        queueType = QueueType::FIFO_QUEUE;
    } else {
        queueType = QueueType::PRIO_QUEUE;
    }

    this->unschRateComp = new UnschedRateComputer(homaConfig,
        homaConfig->useUnschRateInScheduler, 0.1);
    this->trafficPacer = new(this->trafficPacer) TrafficPacer(prioResolver,
        unschRateComp, homaConfig);
    this->grantTimer = grantTimer;
    inboundMsgQueue.initialize(queueType);
    bytesRecvdPerPrio.resize(homaConfig->allPrio);
    scheduledBytesPerPrio.resize(homaConfig->allPrio);
    unschedToReceivePerPrio.resize(homaConfig->allPrio);
    std::fill(bytesRecvdPerPrio.begin(), bytesRecvdPerPrio.end(), 0);
    std::fill(scheduledBytesPerPrio.begin(), scheduledBytesPerPrio.end(), 0);
    std::fill(unschedToReceivePerPrio.begin(),
        unschedToReceivePerPrio.end(), 0);
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
    EV_INFO << "Received req/unsched pkt with msgId " << rxPkt->getMsgId() <<
            " from" << rxPkt->getSrcAddr().str() << " to " <<
            rxPkt->getDestAddr().str() << " for " <<
            rxPkt->getUnschedFields().msgByteLen << " bytes." << endl;
    PktType pktType = (PktType)rxPkt->getPktType();
    uint32_t pktUnschedBytes = rxPkt->getDataBytes();
    uint32_t unschedBytesOnWire =
        HomaPkt::getBytesOnWire(pktUnschedBytes, pktType);
    unschRateComp->updateUnschRate(simTime(), unschedBytesOnWire);
    addArrivedBytes(pktType, rxPkt->getPriority(), pktUnschedBytes);
    if (pktType == PktType::UNSCHED_DATA && pktUnschedBytes == 0) {
        throw cRuntimeError("Unsched pkt with no data byte in it is not allowed.");
    }

    // Check if this message already added to incompleteRxMsg. if not create new
    // inbound message in both the msgQueue and incomplete messages.
    // In the common case, req. pkt is arrived before any other unsched packets but we
    // need to account for the rare case that unsched. pkt arrive earlier
    // because of unexpected delay on the req. and so we have to make the new
    // entry in incompleteRxMsgs and inboundMsgQueue.
    InboundMessage* inboundMsg = lookupIncompleteRxMsg(rxPkt);
    if (!inboundMsg) {
        inboundMsg = new InboundMessage(rxPkt, this, homaConfig);
        if (inboundMsg->bytesToGrant > 0) {
            inboundMsgQueue.push(inboundMsg);
        }
        incompleteRxMsgs[rxPkt->getMsgId()].push_front(inboundMsg);

        // There will be a req. packet and possibly other unsched pkts arriving
        // after this unsched. pkt that we need to account for in the
        // trafficPacer.
        std::vector<uint32_t>& prioUnschedBytes =
            rxPkt->getUnschedFields().prioUnschedBytes;
        for (size_t i = 0; i < prioUnschedBytes.size(); i += 2) {
            uint32_t prio = prioUnschedBytes[i];
            uint32_t unschedBytesInPrio = prioUnschedBytes[i+1];
            trafficPacer->unschedPendingBytes(inboundMsg, unschedBytesInPrio,
                pktType, prio);
            addPendingUnschedBytes(pktType, prio, unschedBytesInPrio);
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
        trafficPacer->bytesArrived(inboundMsg, pktUnschedBytes, pktType,
            rxPkt->getPriority());
    }

    // Append the unsched data to the inboundMsg
    inboundMsg->fillinRxBytes(rxPkt->getUnschedFields().firstByte,
            rxPkt->getUnschedFields().lastByte);

    // Add the bytes on wire to the inbound message
    inboundMsg->totalBytesOnWire += unschedBytesOnWire;

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
        throw cRuntimeError("Received unexpected data with msgId %lu from src %s",
                rxPkt->getMsgId(), rxPkt->getSrcAddr().str().c_str());
    }

    incompleteRxMsgs[rxPkt->getMsgId()].remove(inboundMsg);

    uint32_t bytesReceived = (rxPkt->getSchedDataFields().lastByte
            - rxPkt->getSchedDataFields().firstByte + 1);

    // this is a packet we have already committed in the trafficPacer by sending
    // agrant for it. Now the grant for this packet must be returned to the
    // pacer.
    uint32_t totalOnwireBytesReceived = HomaPkt::getBytesOnWire(bytesReceived,
        PktType::SCHED_DATA);
    // Add the bytes on wire to the inbound message
    inboundMsg->totalBytesOnWire += totalOnwireBytesReceived;

    addArrivedBytes(PktType::SCHED_DATA, rxPkt->getPriority(), bytesReceived);
    ASSERT(bytesReceived > 0);
    inboundMsg->bytesGrantedInFlight -= bytesReceived;
    inboundMsg->fillinRxBytes(rxPkt->getSchedDataFields().firstByte,
        rxPkt->getSchedDataFields().lastByte);

    transport->outstandingGrantBytes -= totalOnwireBytesReceived;
    transport->emit(totalOutstandingBytesSignal,
        trafficPacer->getTotalOutstandingBytes());
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
        transport->sxController.sendOrQueue(grantPkt);

        // set the grantTimer for the next grant
        if (grantTimer->isScheduled()) {
            transport->cancelEvent(grantTimer);
        }
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
        HomaConfigDepot* homaConfig, bool computeAvgUnschRate,
        double minAvgTimeWindow)
    : computeAvgUnschRate(computeAvgUnschRate)
    , bytesRecvTime()
    , sumBytes(0)
    , minAvgTimeWindow(minAvgTimeWindow)
    , homaConfig(homaConfig)
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
        ((double)sumBytes*8/timeDuration)*1e-9/homaConfig->nicLinkSpeed;
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
    uint32_t arrivedBytesOnWire =
        HomaPkt::getBytesOnWire(dataBytesInPkt, pktType);
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
        throw cRuntimeError("InboundMsgQueue was constructed with an invalid type.");
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
    , homaConfig(NULL)
    , srcAddr()
    , destAddr()
    , msgIdAtSender(0)
    , bytesToGrant(0)
    , bytesGrantedInFlight(0)
    , bytesToReceive(0)
    , msgSize(0)
    , totalBytesOnWire(0)
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
        ReceiveScheduler* rxScheduler, HomaConfigDepot* homaConfig)
    : rxScheduler(rxScheduler)
    , homaConfig(homaConfig)
    , srcAddr(rxPkt->getSrcAddr())
    , destAddr(rxPkt->getDestAddr())
    , msgIdAtSender(rxPkt->getMsgId())
    , bytesToGrant(0)
    , bytesGrantedInFlight(0)
    , bytesToReceive(0)
    , msgSize(0)
    , totalBytesOnWire(0)
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
        case PktType::UNSCHED_DATA:
            bytesToGrant = rxPkt->getUnschedFields().msgByteLen -
                rxPkt->getUnschedFields().totalUnschedBytes;
            bytesToReceive = rxPkt->getUnschedFields().msgByteLen;
            msgSize = rxPkt->getUnschedFields().msgByteLen;
            totalUnschedBytes = rxPkt->getUnschedFields().totalUnschedBytes;
            msgCreationTime = rxPkt->getUnschedFields().msgCreationTime;
            break;
        default:
            throw cRuntimeError("Can't create inbound message "
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
HomaTransport::InboundMessage::prepareGrant(uint32_t grantSize,
    uint16_t schedPrio)
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

        int grantedBytesOnWire =
            HomaPkt::getBytesOnWire(grantSize, PktType::SCHED_DATA);
        if (allHighPrioInflightBytes + grantedBytesOnWire >
                homaConfig->maxOutstandingRecvBytes) {
            highPrioSchedDelayBytes +=
                (allHighPrioInflightBytes + grantedBytesOnWire -
                homaConfig->maxOutstandingRecvBytes);
        }
    }

    for (size_t i = 0; i <= schedPrio; i++) {
        highPrioSchedDelayBytes += (rxScheduler->scheduledBytesPerPrio[i] -
            this->scheduledBytesPerPrio[i]);
        highPrioSchedDelayBytes += (rxScheduler->unschedToReceivePerPrio[i] -
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
    rxMsg->setMsgBytesOnWire(this->totalBytesOnWire);

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
                homaConfig->nicLinkSpeed) ;
        }
        simtime_t schedDelay = totalSchedTime - minPossibleSchedTime;
        rxMsg->setTransportSchedDelay(schedDelay);
        rxMsg->setTransportSchedPreemptionLag(schedDelay.dbl() -
            highPrioSchedDelayBytes*8*1e-9/homaConfig->nicLinkSpeed);
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
    this->homaConfig = other.homaConfig;
    this->srcAddr = other.srcAddr;
    this->destAddr = other.destAddr;
    this->msgIdAtSender = other.msgIdAtSender;
    this->bytesToGrant = other.bytesToGrant;
    this->bytesToReceive = other.bytesToReceive;
    this->msgSize = other.msgSize;
    this->totalUnschedBytes = other.totalUnschedBytes;
    this->msgCreationTime = other.msgCreationTime;
}
