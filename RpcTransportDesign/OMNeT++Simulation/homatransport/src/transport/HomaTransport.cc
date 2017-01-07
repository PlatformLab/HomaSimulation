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
#include "transport/PriorityResolver.h"
#include "HomaTransport.h"

Define_Module(HomaTransport);

/**
 * Registering all statistics collection signals.
 */
simsignal_t HomaTransport::msgsLeftToSendSignal =
        registerSignal("msgsLeftToSend");
simsignal_t HomaTransport::stabilitySignal =
        registerSignal("stabilitySignal");
simsignal_t HomaTransport::bytesLeftToSendSignal =
        registerSignal("bytesLeftToSend");
simsignal_t HomaTransport::bytesNeedGrantSignal =
        registerSignal("bytesNeedGrant");
simsignal_t HomaTransport::outstandingGrantBytesSignal =
        registerSignal("outstandingGrantBytes");
simsignal_t HomaTransport::totalOutstandingBytesSignal =
        registerSignal("totalOutstandingBytes");
simsignal_t HomaTransport::rxActiveTimeSignal =
        registerSignal("rxActiveTime");
simsignal_t HomaTransport::rxActiveBytesSignal =
        registerSignal("rxActiveBytes");
simsignal_t HomaTransport::oversubTimeSignal =
        registerSignal("oversubscriptionTime");
simsignal_t HomaTransport::oversubBytesSignal =
        registerSignal("oversubscriptionBytes");
simsignal_t HomaTransport::sxActiveTimeSignal =
        registerSignal("sxActiveTime");
simsignal_t HomaTransport::sxActiveBytesSignal =
        registerSignal("sxActiveBytes");
simsignal_t HomaTransport::sxSchedPktDelaySignal =
        registerSignal("sxSchedPktDelay");
simsignal_t HomaTransport::sxUnschedPktDelaySignal =
        registerSignal("sxUnschedPktDelay");

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
    , sendTimer(NULL)
    , emitSignalTimer(NULL)
    , nextEmitSignalTime(SIMTIME_ZERO)
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
    sendTimer = new cMessage("SendTimer");
    sendTimer->setKind(SelfMsgKind::START);
    emitSignalTimer = new cMessage("SignalEmitionTimer");
    nextEmitSignalTime = SIMTIME_ZERO;
    registerTemplatedStats(homaConfig->allPrio);
    distEstimator = new WorkloadEstimator(homaConfig);
    prioResolver = new PriorityResolver(homaConfig, distEstimator);
    //prioResolver->recomputeCbf(homaConfig->cbfCapMsgSize,
    //    homaConfig->boostTailBytesPrio);
    rxScheduler.initialize(homaConfig, prioResolver);
    sxController.initSendController(homaConfig, prioResolver);
    outstandingGrantBytes = 0;
    scheduleAt(simTime(), sendTimer);
}

void
HomaTransport::processStart()
{
    socket.setOutputGate(gate("udpOut"));
    socket.bind(homaConfig->localPort);
    sendTimer->setKind(SelfMsgKind::SEND);
    emitSignalTimer->setKind(SelfMsgKind::EMITTER);
    scheduleAt(simTime(), emitSignalTimer);
}

void
HomaTransport::testAndEmitStabilitySignal()
{
    if (!sxController.outboundMsgMap.size()) {
        if (emitSignalTimer->isScheduled())
            cancelEvent(emitSignalTimer);
        return;
    }

    if (emitSignalTimer->isScheduled()) {
        return;
    }

    simtime_t currentTime = simTime();

    if (currentTime == nextEmitSignalTime) {
        emit(stabilitySignal, sxController.outboundMsgMap.size());
        nextEmitSignalTime = currentTime + homaConfig->signalEmitPeriod;

    } else if (nextEmitSignalTime < currentTime) {
        double offset = (currentTime - nextEmitSignalTime).dbl();
        double intpart, fracpart;
        fracpart = modf(offset / homaConfig->signalEmitPeriod, &intpart);
        intpart = (fracpart == 0.0 ? intpart : intpart+1);
        nextEmitSignalTime += intpart * homaConfig->signalEmitPeriod;
    }
    scheduleAt(nextEmitSignalTime, emitSignalTimer);
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
                rxScheduler.processGrantTimers(msg);
                break;
            case SelfMsgKind::SEND:
                ASSERT(msg == sendTimer);
                sxController.handlePktTransmitEnd();
                sxController.sendOrQueue(msg);
                break;
            case SelfMsgKind::EMITTER:
                ASSERT(msg == emitSignalTimer);
                testAndEmitStabilitySignal();
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
            handleRecvdPkt(check_and_cast<cPacket*>(msg));
        }
    }
}

void
HomaTransport::handleRecvdPkt(cPacket* pkt)
{
    HomaPkt* rxPkt = check_and_cast<HomaPkt*>(pkt);
    // check and set the localAddr
    if (localAddr == inet::L3Address()) {
        localAddr = rxPkt->getDestAddr();
    } else {
        ASSERT(localAddr == rxPkt->getDestAddr());
    }
    // update the owner transport for this packet
    rxPkt->ownerTransport = this;
    emit(priorityStatsSignals[rxPkt->getPriority()], rxPkt);

    // update active period stats
    uint32_t pktLenOnWire = HomaPkt::getBytesOnWire(rxPkt->getDataBytes(),
        (PktType)rxPkt->getPktType());
    simtime_t pktDuration =
        SimTime(1e-9 * (pktLenOnWire * 8.0 / homaConfig->nicLinkSpeed));
    if (rxScheduler.getInflightBytes() == 0) {
        // We were not in an active period prior to this packet but entered in
        // one starting this packet.
        rxScheduler.activePeriodStart = simTime() - pktDuration;
        ASSERT(rxScheduler.rcvdBytesPerActivePeriod == 0);
        rxScheduler.rcvdBytesPerActivePeriod = 0;
    }
    rxScheduler.rcvdBytesPerActivePeriod += pktLenOnWire;

    // handle data or grant grant packets appropriately
    switch (rxPkt->getPktType()) {
        case PktType::REQUEST:
        case PktType::UNSCHED_DATA:
        case PktType::SCHED_DATA:
            rxScheduler.processReceivedPkt(rxPkt);
            break;

        case PktType::GRANT:
            sxController.processReceivedGrant(rxPkt);
            break;

        default:
            throw cRuntimeError(
                "Received packet type(%d) is not valid.",
                rxPkt->getPktType());
    }

    // Check if this is the end of active period and we should dump stats for
    // wasted bandwidth
    if (rxScheduler.getInflightBytes() == 0) {
        // This is end of a active period, so we should dump the stats and reset
        // varibles that track next active period.
        emit(rxActiveTimeSignal, simTime() - rxScheduler.activePeriodStart);
        emit(rxActiveBytesSignal, rxScheduler.rcvdBytesPerActivePeriod);
        rxScheduler.rcvdBytesPerActivePeriod = 0;
        rxScheduler.activePeriodStart = simTime();
    }

}

void
HomaTransport::finish()
{
    cancelAndDelete(sendTimer);
    cancelAndDelete(emitSignalTimer);
}


/**
 * HomaTransport::SendController
 */
HomaTransport::SendController::SendController(HomaTransport* transport)
    : bytesLeftToSend(0)
    , bytesNeedGrant(0)
    , msgId(0)
    , sentPkt()
    , sentPktDuration(SIMTIME_ZERO)
    , outboundMsgMap()
    , rxAddrMsgMap()
    , unschedByteAllocator(NULL)
    , prioResolver(NULL)
    , outbndMsgSet()
    , transport(transport)
    , homaConfig(NULL)
    , activePeriodStart(SIMTIME_ZERO)
    , sentBytesPerActivePeriod(0)

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
HomaTransport::SendController::handlePktTransmitEnd()
{
    for (auto it = rxAddrMsgMap.begin(); it != rxAddrMsgMap.end(); ++it) {
        uint32_t lastDestAddr = sentPkt.getDestAddr().toIPv4().getInt();
        uint32_t rxAddr = it->first;
        if (rxAddr != lastDestAddr) {
            auto allMsgToRxAddr = it->second;
            uint64_t totalTrailingUnsched = 0;
            simtime_t oldestSchedPkt = simTime();
            simtime_t currentTime = simTime();
            for (auto itt = allMsgToRxAddr.begin();
                    itt != allMsgToRxAddr.end(); ++itt) {
                OutboundMessage* msg = *itt;
                if (msg->msgSize > msg->bytesLeft) {
                    // Unsched delay at sender may waste rx bw only if msg has
                    // already sent at least one request packet.
                    totalTrailingUnsched += msg->unschedBytesLeft;
                }
                auto& schedPkts = msg->getTxSchedPkts();
                // Find oldest sched pkt creation time
                for (auto ittt = schedPkts.begin(); ittt != schedPkts.end();
                        ++ittt) {
                    simtime_t pktCreationTime = (*ittt)->getCreationTime();
                    oldestSchedPkt = (pktCreationTime < oldestSchedPkt
                        ? pktCreationTime : oldestSchedPkt);
                }
            }

            simtime_t bubbleCausedAtRecvr;
            if (totalTrailingUnsched > 0) {
                simtime_t totalUnschedTxTimeLeft = SimTime(1e-9 * (
                    HomaPkt::getBytesOnWire(totalTrailingUnsched,
                    PktType::UNSCHED_DATA) * 8.0 / homaConfig->nicLinkSpeed));
                bubbleCausedAtRecvr = sentPktDuration < totalUnschedTxTimeLeft
                    ? sentPktDuration : totalUnschedTxTimeLeft;
                transport->emit(sxUnschedPktDelaySignal, bubbleCausedAtRecvr);
            }

            if (oldestSchedPkt != currentTime) {
                bubbleCausedAtRecvr =
                    sentPktDuration < currentTime - oldestSchedPkt
                    ? sentPktDuration : currentTime - oldestSchedPkt;
                transport->emit(sxSchedPktDelaySignal, bubbleCausedAtRecvr);
            }
        }
    }
}

void
HomaTransport::SendController::processSendMsgFromApp(AppMessage* sendMsg)
{
    transport->emit(msgsLeftToSendSignal, outboundMsgMap.size());
    transport->emit(bytesLeftToSendSignal, bytesLeftToSend);
    transport->emit(bytesNeedGrantSignal, bytesNeedGrant);
    uint32_t destAddr = sendMsg->getDestAddr().toIPv4().getInt();
    uint32_t msgSize = sendMsg->getByteLength();
    std::vector<uint16_t> reqUnschedDataVec =
        unschedByteAllocator->getReqUnschedDataPkts(destAddr, msgSize);

    outboundMsgMap.emplace(std::piecewise_construct,
            std::forward_as_tuple(msgId),
            std::forward_as_tuple(sendMsg, this, msgId, reqUnschedDataVec));
    OutboundMessage* outboundMsg = &(outboundMsgMap.at(msgId));
    rxAddrMsgMap[destAddr].insert(outboundMsg);

    if (bytesLeftToSend == 0) {
        // We have just entered in an active transmit period.
        activePeriodStart = simTime();
        ASSERT(sentBytesPerActivePeriod == 0);
        sentBytesPerActivePeriod = 0;
    }
    bytesLeftToSend += outboundMsg->getBytesLeft();
    transport->testAndEmitStabilitySignal();
    outboundMsg->prepareRequestAndUnsched();
    auto insResult = outbndMsgSet.insert(outboundMsg);
    ASSERT(insResult.second); // check insertion took place
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
    int numBytesSent = bytesToSchedOld - bytesToSchedNew;
    bytesNeedGrant -= numBytesSent;
    ASSERT(numBytesSent > 0 && bytesLeftToSend >= 0 && bytesNeedGrant >= 0);
    auto insResult = outbndMsgSet.insert(outboundMsg);
    ASSERT(insResult.second); // check insertion took place
    delete rxPkt;
    sendOrQueue();
}

void
HomaTransport::SendController::sendOrQueue(cMessage* msg)
{
    HomaPkt* sxPkt = NULL;
    if (msg == transport->sendTimer) {
        ASSERT(msg->getKind() == SelfMsgKind::SEND);
        if (!outGrantQueue.empty()) {
            sxPkt = outGrantQueue.top();
            outGrantQueue.pop();
            sendPktAndScheduleNext(sxPkt);
            return;
        }

        if (!outbndMsgSet.empty()) {
            OutboundMessage* highPrioMsg = *(outbndMsgSet.begin());
            size_t numRemoved =  outbndMsgSet.erase(highPrioMsg);
            ASSERT(numRemoved == 1); // check only one msg is removed
            bool hasMoreReadyPkt = highPrioMsg->getTransmitReadyPkt(&sxPkt);
            sendPktAndScheduleNext(sxPkt);
            if (highPrioMsg->getBytesLeft() <= 0) {
                ASSERT(!hasMoreReadyPkt);
                msgTransmitComplete(highPrioMsg);
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
            sendPktAndScheduleNext(sxPkt);
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
    sendPktAndScheduleNext(sxPkt);
    if (highPrioMsg->getBytesLeft() <= 0) {
        ASSERT(!hasMoreReadyPkt);
        msgTransmitComplete(highPrioMsg);
        return;
    }

    if (hasMoreReadyPkt) {
        auto insResult = outbndMsgSet.insert(highPrioMsg);
        ASSERT(insResult.second); // check insertion took place
        return;
    }
}

void
HomaTransport::SendController::sendPktAndScheduleNext(HomaPkt* sxPkt)
{
    PktType pktType = (PktType)sxPkt->getPktType();
    uint32_t numDataBytes = sxPkt->getDataBytes();
    uint32_t bytesSentOnWire = HomaPkt::getBytesOnWire(numDataBytes, pktType);
    simtime_t currentTime = simTime();
    simtime_t sxPktDuration =  SimTime(1e-9 * (bytesSentOnWire * 8.0 /
        homaConfig->nicLinkSpeed));

    switch(pktType)
    {
        case PktType::REQUEST:
        case PktType::UNSCHED_DATA:
        case PktType::SCHED_DATA:
            ASSERT(bytesLeftToSend >= numDataBytes);
            bytesLeftToSend -= numDataBytes;
            sentBytesPerActivePeriod += bytesSentOnWire;
            if (bytesLeftToSend == 0) {
                // This is end of the active period, so record stats
                simtime_t activePeriod = currentTime - activePeriodStart +
                    sxPktDuration;
                transport->emit(sxActiveBytesSignal, sentBytesPerActivePeriod);
                transport->emit(sxActiveTimeSignal, activePeriod);

                // reset stats tracking variables
                sentBytesPerActivePeriod = 0;
                activePeriodStart = currentTime;
            }
            break;

        case PktType::GRANT:
            // Add the length of the grant packet to the bytes
            // sent in active period.
            if (bytesLeftToSend > 0) {
                // SendController is already in an active period. No need to
                // record stats.
                sentBytesPerActivePeriod += bytesSentOnWire;
            } else {
                transport->emit(sxActiveBytesSignal, bytesSentOnWire);
                transport->emit(sxActiveTimeSignal, sxPktDuration);
            }
            break;

        default:
            throw cRuntimeError("SendController::sendPktAndScheduleNext: "
                "packet to send has unknown pktType %d.", pktType);
    }

    simtime_t nextSendTime = sxPktDuration + currentTime;
    sentPkt = *sxPkt;
    sentPktDuration = sxPktDuration;
    transport->socket.sendTo(sxPkt, sxPkt->getDestAddr(), homaConfig->destPort);
    transport->scheduleAt(nextSendTime, transport->sendTimer);
}

void
HomaTransport::SendController::msgTransmitComplete(OutboundMessage* msg)
{
    uint32_t destIp = msg->destAddr.toIPv4().getInt();
    rxAddrMsgMap.at(destIp).erase(msg);
    if (rxAddrMsgMap.at(destIp).empty()) {
        rxAddrMsgMap.erase(destIp);
    }
    outboundMsgMap.erase(msg->getMsgId());
    transport->testAndEmitStabilitySignal();
    return;
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
            if (q2.empty()) {
                if (q1.empty()) {
                    return (msg1->getMsgCreationTime() <
                        msg2->getMsgCreationTime()) ||
                        (msg1->getMsgCreationTime() ==
                        msg2->getMsgCreationTime() &&
                         msg1->getMsgId() < msg2->getMsgId());
                } else {
                    return true;
                }
            } else if (q1.empty()) {
                return false;
            } else {
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
    , unschedBytesLeft(0)
    , nextExpectedUnschedTxTime(SIMTIME_ZERO)
    , reqUnschedDataVec(reqUnschedDataVec)
    , destAddr(outMsg->getDestAddr())
    , srcAddr(outMsg->getSrcAddr())
    , msgCreationTime(outMsg->getMsgCreationTime())
    , txPkts()
    , txSchedPkts()
    , sxController(sxController)
    , homaConfig(sxController->homaConfig)
{}

HomaTransport::OutboundMessage::OutboundMessage()
    : msgId(0)
    , msgSize(0)
    , bytesToSched(0)
    , nextByteToSched(0)
    , bytesLeft(0)
    , unschedBytesLeft(0)
    , nextExpectedUnschedTxTime(SIMTIME_ZERO)
    , reqUnschedDataVec({0})
    , destAddr()
    , srcAddr()
    , msgCreationTime(SIMTIME_ZERO)
    , txPkts()
    , txSchedPkts()
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
    this->unschedBytesLeft = other.unschedBytesLeft;
    this->nextExpectedUnschedTxTime = other.nextExpectedUnschedTxTime;
    this->reqUnschedDataVec = other.reqUnschedDataVec;
    this->destAddr = other.destAddr;
    this->srcAddr = other.srcAddr;
    this->msgCreationTime = other.msgCreationTime;
}

void
HomaTransport::OutboundMessage::prepareRequestAndUnsched()
{
    std::vector<uint16_t> unschedPrioVec =
        sxController->prioResolver->getUnschedPktsPrio(this);

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
    unschedBytesLeft = totalUnschedBytes;

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
            if (unschedFields.prioUnschedBytes[j]==unschedPkt->getPriority()) {
                // find the two elements in this prioUnschedBytes vec that
                // corresponds to the priority of this packet and subtract
                // the bytes in this packet from prioUnschedBytes.
                unschedFields.prioUnschedBytes[j+1]-=this->reqUnschedDataVec[i];
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
    txSchedPkts.insert(dataPkt);

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
 * Among all packets ready for transmission for this message, this function
 * retrieves highest priority packet from sender's perspective for this message.
 * Call this function only when ready to send the packet out.
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
    PktType outPktType = (PktType)head->getPktType();
    txPkts.pop();
    if (outPktType == PktType::SCHED_DATA) {
        txSchedPkts.erase(head);
    }

    *outPkt = head;
    uint32_t numDataBytes = head->getDataBytes();
    bytesLeft -= numDataBytes;

    if (outPktType == PktType::REQUEST || outPktType == PktType::UNSCHED_DATA) {
        unschedBytesLeft -= numDataBytes;
    }

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
    , unschRateComp(NULL)
    , ipSendersMap()
    , grantTimersMap()
    , schedSenders(NULL)
    , inflightUnschedPerPrio()
    , inflightSchedPerPrio()
    , inflightSchedBytes(0)
    , inflightUnschedBytes(0)
    , bytesRecvdPerPrio()
    , scheduledBytesPerPrio()
    , unschedToRecvPerPrio()
    , allBytesRecvd(0)
    , unschedBytesToRecv(0)
    , activePeriodStart(SIMTIME_ZERO)
    , rcvdBytesPerActivePeriod(0)
    , oversubPeriodStart(SIMTIME_ZERO)
    , oversubPeriodStop(SIMTIME_ZERO)
    , inOversubPeriod(false)
    , rcvdBytesPerOversubPeriod(0)
{}

void
HomaTransport::ReceiveScheduler::initialize(HomaConfigDepot* homaConfig,
    PriorityResolver* prioResolver)
{
    this->homaConfig = homaConfig;
    this->unschRateComp = new UnschedRateComputer(homaConfig,
        homaConfig->useUnschRateInScheduler, 0.1);
    this->schedSenders = new SchedSenders(homaConfig);
    inflightUnschedPerPrio.resize(this->homaConfig->allPrio);
    inflightSchedPerPrio.resize(this->homaConfig->allPrio);
    bytesRecvdPerPrio.resize(homaConfig->allPrio);
    scheduledBytesPerPrio.resize(homaConfig->allPrio);
    unschedToRecvPerPrio.resize(homaConfig->allPrio);
    std::fill(inflightUnschedPerPrio.begin(), inflightUnschedPerPrio.end(), 0);
    std::fill(inflightSchedPerPrio.begin(), inflightSchedPerPrio.end(), 0);
    std::fill(bytesRecvdPerPrio.begin(), bytesRecvdPerPrio.end(), 0);
    std::fill(scheduledBytesPerPrio.begin(), scheduledBytesPerPrio.end(), 0);
    std::fill(unschedToRecvPerPrio.begin(), unschedToRecvPerPrio.end(), 0);
}

HomaTransport::ReceiveScheduler::~ReceiveScheduler()
{
    // Iterate through all incomplete messages and delete them
    for (auto it = grantTimersMap.begin(); it != grantTimersMap.end(); it++) {
        cMessage* grantTimer = it->first;
        transport->cancelAndDelete(grantTimer);
    }

    for (auto it = ipSendersMap.begin(); it != ipSendersMap.end(); it++) {
        SenderState* s = it->second;
        for (auto itt = s->incompleteMesgs.begin();
                itt != s->incompleteMesgs.end(); it++) {
            InboundMessage* mesg = itt->second;
            delete mesg;
        }
        delete s;
    }
    delete unschRateComp;
}

HomaTransport::InboundMessage*
HomaTransport::ReceiveScheduler::lookupInboundMesg(HomaPkt* rxPkt) const
{
    // Get the SenderState collection for the sender of this pkt
    inet::IPv4Address srcIp = rxPkt->getSrcAddr().toIPv4();
    auto iter = ipSendersMap.find(srcIp.getInt());
    if (iter == ipSendersMap.end()) {
        return NULL;
    }
    SenderState* s = iter->second;

    // Find inboundMesg in s
    auto mesgIt = s->incompleteMesgs.find(rxPkt->getMsgId());
    if (mesgIt == s->incompleteMesgs.end()) {
        return NULL;
    }
    return mesgIt->second;
}

/**
 *
 */
void
HomaTransport::ReceiveScheduler::processReceivedPkt(HomaPkt* rxPkt)
{
    uint32_t pktLenOnWire = HomaPkt::getBytesOnWire(rxPkt->getDataBytes(),
        (PktType)rxPkt->getPktType());
    simtime_t pktDuration =
        SimTime(1e-9 * (pktLenOnWire * 8.0 / homaConfig->nicLinkSpeed));

    // check and update states for oversubscription time and bytes.
    if (schedSenders->numSenders > schedSenders->numToGrant) {
        // Already in an oversubcription period
        ASSERT(inOversubPeriod && oversubPeriodStop==MAXTIME &&
            oversubPeriodStart < simTime()-pktDuration);
        rcvdBytesPerOversubPeriod += pktLenOnWire;
    } else if (oversubPeriodStop != MAXTIME) {
        // an oversubscription period has recently been marked ended and we need
        // to emit signal to record the stats.
        ASSERT(!inOversubPeriod && oversubPeriodStop <= simTime());
        simtime_t oversubDuration = oversubPeriodStop-oversubPeriodStart;
        simtime_t oversubOffset = oversubPeriodStop-simTime()+pktDuration;
        if (oversubOffset > 0) {
            oversubDuration += oversubOffset;
            rcvdBytesPerOversubPeriod += (uint64_t)(
                oversubOffset.dbl() * homaConfig->nicLinkSpeed * 1e9 / 8);
        }
        transport->emit(oversubTimeSignal, oversubDuration);
        transport->emit(oversubBytesSignal, rcvdBytesPerOversubPeriod);

        // reset the states and variables
        rcvdBytesPerOversubPeriod = 0;
        oversubPeriodStart = MAXTIME;
        oversubPeriodStop = MAXTIME;
        inOversubPeriod = false;
    }

    // Get the SenderState collection for the sender of this pkt
    inet::IPv4Address srcIp = rxPkt->getSrcAddr().toIPv4();
    auto iter = ipSendersMap.find(srcIp.getInt());
    SenderState* s;
    if (iter == ipSendersMap.end()) {
        // Construct new SenderState if pkt arrived from new sender
        char timerName[50];
        sprintf(timerName, "senderIP%s", srcIp.str().c_str());
        cMessage* grantTimer = new cMessage(timerName);
        grantTimer->setKind(SelfMsgKind::GRANT);
        s = new SenderState(srcIp, this, grantTimer, homaConfig);
        grantTimersMap[grantTimer] = s;
        ipSendersMap[srcIp.getInt()] = s;
    } else {
        s = iter->second;
    }
    // At each new pkt arrival, the order of senders in the schedSenders list
    // can change and/or a mesg might need to get a new grant
    int sIndOld = schedSenders->remove(s);
    int mesgIdxInS = s->handleInboundPkt(rxPkt);
    EV_INFO << "remove sender at ind " << sIndOld << "and handled mesg at ind "
        << mesgIdxInS;
    auto ret = schedSenders->insPoint(s);
    int sInd = ret.first;
    int headInd = ret.second;
    schedSenders->handleGrantRequest(s, sInd, headInd);

    // check and update states for oversubscription time and bytes.
    if (schedSenders->numSenders <= schedSenders->numToGrant) {
        if (inOversubPeriod) {
            // oversubscription period ended. emit signals and record stats.
            transport->emit(oversubTimeSignal, simTime() - oversubPeriodStart);
            transport->emit(oversubBytesSignal, rcvdBytesPerOversubPeriod);
        }
        // reset the states and variables
        rcvdBytesPerOversubPeriod = 0;
        inOversubPeriod = false;
        oversubPeriodStart = MAXTIME;
        oversubPeriodStop = MAXTIME;

    } else if (!inOversubPeriod) {
        // mark the start of a oversubcription period
        ASSERT(rcvdBytesPerOversubPeriod == 0 &&
            oversubPeriodStart == MAXTIME && oversubPeriodStop == MAXTIME);
        inOversubPeriod = true;
        oversubPeriodStart = simTime() - pktDuration;
        oversubPeriodStop = MAXTIME;
        rcvdBytesPerOversubPeriod += pktLenOnWire;
    }
    delete rxPkt;
}

void
HomaTransport::ReceiveScheduler::processGrantTimers(cMessage* grantTimer)
{
    SenderState* s = grantTimersMap[grantTimer];
    schedSenders->remove(s);
    auto ret = schedSenders->insPoint(s);
    int sInd = ret.first;
    int headInd = ret.second;
    schedSenders->handleGrantRequest(s, sInd, headInd);
    if (inOversubPeriod &&
            schedSenders->numSenders <= schedSenders->numToGrant) {
        // Receiver was in an oversubscription period which is now ended after
        // sending the most recent grant. Mark the end of oversubscription
        // period.
        inOversubPeriod = false;
        oversubPeriodStop = simTime();
    }

}

/**
 * HomaTransport::ReceiveScheduler::SenderState
 */
HomaTransport::ReceiveScheduler::SenderState::SenderState(
        inet::IPv4Address srcAddr, ReceiveScheduler* rxScheduler,
        cMessage* grantTimer, HomaConfigDepot* homaConfig)
    : homaConfig(homaConfig)
    , rxScheduler(rxScheduler)
    , grantTimer(grantTimer)
    , senderAddr(srcAddr)
    , mesgsToGrant()
    , incompleteMesgs()
    , lastGrantPrio(homaConfig->allPrio)
    , lastIdx(homaConfig->adaptiveSchedPrioLevels)
{}

simtime_t
HomaTransport::ReceiveScheduler::SenderState::getNextGrantTime(
    simtime_t currentTime, uint32_t grantSize)
{
    uint32_t grantedPktSizeOnWire =
        HomaPkt::getBytesOnWire(grantSize, PktType::SCHED_DATA);

    simtime_t nextGrantTime = SimTime(1e-9 * (grantedPktSizeOnWire * 8.0 /
        homaConfig->nicLinkSpeed)) + currentTime;
    return nextGrantTime;
}

/**
 * return -1, if rxPkt belong to a mesg that doesn't need (anymore?) grant.
 * Returns 0 if rxPkt belongs to a the most preferred scheduled mesg of the
 * sender that needs grant. Return +1 if rxPkt belongs to a scheduled mesg of
 * sender but the mesg is not the most preffered scheduled mesg for the sender.
 */
int
HomaTransport::ReceiveScheduler::SenderState::handleInboundPkt(HomaPkt* rxPkt)
{
    uint32_t pktData =  rxPkt->getDataBytes();
    PktType pktType = (PktType)rxPkt->getPktType();
    InboundMessage* inboundMesg = NULL;
    auto mesgIt = incompleteMesgs.find(rxPkt->getMsgId());
    rxScheduler->addArrivedBytes(pktType, rxPkt->getPriority(), pktData);
    if (mesgIt == incompleteMesgs.end()) {
        ASSERT(rxPkt->getPktType() == PktType::REQUEST ||
            rxPkt->getPktType() == PktType::UNSCHED_DATA);
        inboundMesg = new InboundMessage(rxPkt, this->rxScheduler, homaConfig);
        incompleteMesgs[rxPkt->getMsgId()] = inboundMesg;

        // There will be other unsched pkts arriving after this unsched. pkt
        // that we need to account for them.
        std::vector<uint32_t>& prioUnschedBytes =
            rxPkt->getUnschedFields().prioUnschedBytes;
        for (size_t i = 0; i < prioUnschedBytes.size(); i += 2) {
            uint32_t prio = prioUnschedBytes[i];
            uint32_t unschedBytesInPrio = prioUnschedBytes[i+1];
            rxScheduler->addPendingUnschedBytes(pktType, prio,
                unschedBytesInPrio);
        }
        rxScheduler->transport->emit(totalOutstandingBytesSignal,
                rxScheduler->getInflightBytes());
        inboundMesg->updatePerPrioStats();

    } else {
        inboundMesg = mesgIt->second;
        if (inboundMesg->bytesToGrant > 0) {
            size_t numRemoved = mesgsToGrant.erase(inboundMesg);
            ASSERT(numRemoved == 1);
        }
        // this is a packet that we expected to be received and has previously
        // accounted for in inflightBytes.
        rxScheduler->pendingBytesArrived(pktType, rxPkt->getPriority(),
            pktData);
        rxScheduler->transport->emit(
            totalOutstandingBytesSignal, rxScheduler->getInflightBytes());

    }
    ASSERT(rxPkt->getMsgId() == inboundMesg->msgIdAtSender &&
        rxPkt->getSrcAddr().toIPv4() == senderAddr);

    // Append the unsched data to the inboundMesg and update variables
    // tracking inflight bytes and total bytes
    if (pktType == PktType::REQUEST || pktType == PktType::UNSCHED_DATA) {
        inboundMesg->fillinRxBytes(rxPkt->getUnschedFields().firstByte,
                rxPkt->getUnschedFields().lastByte, pktType);

    } else if (pktType == PktType::SCHED_DATA) {
        inboundMesg->fillinRxBytes(rxPkt->getSchedDataFields().firstByte,
            rxPkt->getSchedDataFields().lastByte, pktType);

    } else {
        cRuntimeError("PktType %d is not recongnized", rxPkt->getPktType());
    }

    if (inboundMesg->bytesToReceive == 0) {
        ASSERT(inboundMesg->bytesToGrant == 0);

        // this message is complete, so send it to the application
        AppMessage* rxMsg = inboundMesg->prepareRxMsgForApp();
        rxScheduler->transport->send(rxMsg, "appOut", 0);

        // remove this message from the incompleteRxMsgs
        incompleteMesgs.erase(inboundMesg->msgIdAtSender);
        delete inboundMesg;
        return -1;
    } else if (inboundMesg->bytesToGrant == 0) {

        // no grant needed for this mesg but mesg is not complete yet byt we
        // still need to keep the message until all of its packets has arrived
        return -1;
    } else {
        auto resPair = mesgsToGrant.insert(inboundMesg);
        ASSERT(resPair.second);
        return resPair.first == mesgsToGrant.begin() ? 0 : 1;
    }
}

int
HomaTransport::ReceiveScheduler::SenderState::sendAndScheduleGrant(
        uint32_t grantPrio)
{
    simtime_t currentTime = simTime();
    auto topMesgIt = mesgsToGrant.begin();
    ASSERT(topMesgIt != mesgsToGrant.end());
    InboundMessage* topMesg = *topMesgIt;
    ASSERT(topMesg->bytesToGrant > 0);
    uint32_t newIdx = grantPrio - homaConfig->allPrio +
        homaConfig->adaptiveSchedPrioLevels;
    
    // if prioresolver gives a better prio, use that one
    uint16_t resolverPrio =
        rxScheduler->transport->prioResolver->getSchedPktPrio(topMesg);
    grantPrio = std::min(grantPrio, (uint32_t)resolverPrio);
    if (grantTimer->isScheduled()){
        if (lastGrantPrio <= grantPrio) {
            return 0;
        }
        rxScheduler->transport->cancelEvent(grantTimer);
    }

    if (topMesg->totalBytesInFlight >= homaConfig->maxOutstandingRecvBytes) {
        return 0;
    }

    uint32_t grantSize =
        std::min(topMesg->bytesToGrant, homaConfig->grantMaxBytes);
    HomaPkt* grantPkt = topMesg->prepareGrant(grantSize, grantPrio);
    lastGrantPrio = grantPrio;
    lastIdx = newIdx;
    // update stats and send grant
    grantSize = grantPkt->getGrantFields().grantBytes;
    uint16_t prio = grantPkt->getGrantFields().schedPrio;
    rxScheduler->addPendingGrantedBytes(prio, grantSize);
    topMesg->updatePerPrioStats();
    rxScheduler->transport->sxController.sendOrQueue(grantPkt);
    rxScheduler->transport->emit(outstandingGrantBytesSignal,
        rxScheduler->transport->outstandingGrantBytes);

    if (topMesg->bytesToGrant > 0) {
        if (topMesg->totalBytesInFlight < homaConfig->maxOutstandingRecvBytes) {
            rxScheduler->transport->scheduleAt(
            getNextGrantTime(currentTime, grantSize), grantTimer);
        }
        return topMesg->bytesToGrant;
    }
    mesgsToGrant.erase(topMesgIt);
    if (mesgsToGrant.empty()) {
        return -1;
    }
    topMesgIt = mesgsToGrant.begin();
    ASSERT(topMesgIt != mesgsToGrant.end());
    InboundMessage* newTopMesg = *topMesgIt;
    ASSERT(newTopMesg->bytesToGrant > 0);

    if (newTopMesg->totalBytesInFlight < homaConfig->maxOutstandingRecvBytes) {
        rxScheduler->transport->scheduleAt(
            getNextGrantTime(simTime(), grantSize), grantTimer);
    }
    return newTopMesg->bytesToGrant;
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
HomaTransport::ReceiveScheduler::addPendingGrantedBytes(uint16_t prio,
    uint32_t grantedBytes)
{
    uint32_t schedBytesOnWire =
        HomaPkt::getBytesOnWire(grantedBytes, PktType::SCHED_DATA);
    scheduledBytesPerPrio.at(prio) += schedBytesOnWire;
    inflightSchedBytes += schedBytesOnWire;
    inflightSchedPerPrio.at(prio) += schedBytesOnWire;
}

void
HomaTransport::ReceiveScheduler::addPendingUnschedBytes(PktType pktType,
    uint16_t prio, uint32_t bytesToArrive)
{
    uint32_t unschedBytesOnWire =
        HomaPkt::getBytesOnWire(bytesToArrive, pktType);
    unschedToRecvPerPrio.at(prio) += unschedBytesOnWire;
    inflightUnschedBytes += unschedBytesOnWire;
    inflightUnschedPerPrio.at(prio) += unschedBytesOnWire;
}

void
HomaTransport::ReceiveScheduler::pendingBytesArrived(PktType pktType,
    uint16_t prio, uint32_t dataBytesInPkt)
{
    uint32_t arrivedBytesOnWire =
        HomaPkt::getBytesOnWire(dataBytesInPkt, pktType);
    switch(pktType)
    {
        case PktType::REQUEST:
        case PktType::UNSCHED_DATA:
            inflightUnschedBytes -= arrivedBytesOnWire;
            inflightUnschedPerPrio.at(prio) -= arrivedBytesOnWire;
            break;
        case PktType::SCHED_DATA:
            inflightSchedBytes -= arrivedBytesOnWire;
            inflightSchedPerPrio.at(prio) -= arrivedBytesOnWire;
            break;
        default:
            throw cRuntimeError("Unknown pktType %d", pktType);
    }
}

/**
 * HomaTransport::ReceiveScheduler::SchedSenders
 */
HomaTransport::ReceiveScheduler::SchedSenders::SchedSenders(
        HomaConfigDepot* homaConfig)
    : senders()
    , schedPrios(homaConfig->adaptiveSchedPrioLevels)
    , numToGrant(homaConfig->numSendersToKeepGranted)
    , headIdx(schedPrios)
    , numSenders(0)
    , homaConfig(homaConfig)
{
    ASSERT(schedPrios >= numToGrant);
    for (size_t i = 0; i < schedPrios; i++) {
        senders.push_back(NULL);
    }
}

/**
 *
 */
void
HomaTransport::ReceiveScheduler::SchedSenders::handleGrantRequest(
    SenderState* s, int sInd, int headInd)
{
    if (sInd < 0)
        return;

    if (sInd - headInd >= numToGrant) {
        insert(s);
        return;
    }

    s->sendAndScheduleGrant(
        sInd + homaConfig->allPrio - homaConfig->adaptiveSchedPrioLevels);
    auto ret = insPoint(s);
    int sIndNew = ret.first;
    int headIndNew = ret.second;
    if (sIndNew >= 0 && sIndNew - headIndNew < numToGrant) {
        insert(s);
        return;
    }

    if (sIndNew > 0)
        insert(s);
    s = removeAt(headIdx + numToGrant - 1);
    if (!s)
        return;

    auto retNew = insPoint(s);
    int sIndNewNew = retNew.first;
    int headIndNewNew = retNew.second;
    handleGrantRequest(s, sIndNewNew, headIndNewNew);
}

/**
 * negative value means the mesg didn't belong to the schedSenders. Otherwise,
 * it returns the index at which the s was removed from.
 */
int
HomaTransport::ReceiveScheduler::SchedSenders::remove(SenderState* s)
{
    if (numSenders == 0) {
        senders.clear();
        headIdx = schedPrios;
        for (size_t i = 0; i < schedPrios; i++) {
            senders.push_back(NULL);
        }
        ASSERT(s->mesgsToGrant.empty());
        return -1;
    }
    if (s->mesgsToGrant.empty()) {
        return -1;
    }

    CompSched cmpSched;
    std::vector<SenderState*>::iterator headIt = headIdx + senders.begin();
    std::vector<SenderState*>::iterator lastIt = headIt + numSenders;
    std::vector<SenderState*>::iterator nltIt =
        std::lower_bound(headIt, lastIt, s, cmpSched);
    if (nltIt != lastIt && (!cmpSched(*nltIt, s) && !cmpSched(s, *nltIt))) {
        ASSERT(s == *nltIt);
        numSenders--;
        int ind = nltIt - senders.begin();
        if (ind == (int)headIdx &&
                (ind+std::min(numSenders, (uint32_t)numToGrant) < schedPrios)) {
            senders[ind] = NULL;
            headIdx++;
        } else {
            senders.erase(nltIt);
        }
        return ind;
    } else {
        throw cRuntimeError("Trying to remove an unlisted sched sender!");
    }
}

HomaTransport::ReceiveScheduler::SenderState*
HomaTransport::ReceiveScheduler::SchedSenders::removeAt(uint32_t rmIdx)
{
    if (rmIdx < headIdx || rmIdx >= (headIdx + numSenders)) {
        EV_INFO << "Objects are within range [" << headIdx << ", " <<
            headIdx + numSenders << "). Can't remove at rmIdx " << rmIdx;
        return NULL;
    }
    numSenders--;
    SenderState* rmvd = senders[rmIdx];
    ASSERT(rmvd);
    if (rmIdx == headIdx &&
            (rmIdx+std::min(numSenders, (uint32_t)numToGrant) < schedPrios)) {
        senders[rmIdx] = NULL;
        headIdx++;
    } else {
        senders.erase(senders.begin()+rmIdx);
    }
    return rmvd;
}

/**
 * If the element already exists in the list, insertion will not happen and
 * returns false. Otherwise, element will be inserted at the position
 * insId returned from insPoint method.
 */
void
HomaTransport::ReceiveScheduler::SchedSenders::insert(SenderState* s)
{
    ASSERT(!s->mesgsToGrant.empty());
    auto ins = insPoint(s);
    size_t insIdx = ins.first;
    size_t newHeadIdx = ins.second;

    for (int i = 0; i < (int)headIdx - 1; i++) {
        ASSERT(!senders[i]);
    }
    ASSERT(newHeadIdx <= headIdx);
    size_t leftShift = headIdx - newHeadIdx;
    senders.erase(senders.begin(), senders.begin()+leftShift);
    numSenders++;
    headIdx = newHeadIdx;
    //uint64_t mesgSize = (*s->mesgsToGrant.begin())->msgSize;
    //uint64_t toGrant = (*s->mesgsToGrant.begin())->bytesToGrant;
    //std::cout << (*s->mesgsToGrant.begin())->msgSize << ", "
    //<<(*s->mesgsToGrant.begin())->bytesToGrant << std::endl;
    senders.insert(senders.begin()+insIdx, s);
    return;
}

/**
 * returns a pair (insertIndex, headIndex). insertIndex negative, means s
 * doesn't belong to the schedSenders
 */
std::pair<int, int>
HomaTransport::ReceiveScheduler::SchedSenders::insPoint(SenderState* s)
{
    CompSched cmpSched;
    if (s->mesgsToGrant.empty()) {
        return std::make_pair(-1, headIdx);
    }
    std::vector<SenderState*>::iterator headIt = headIdx + senders.begin();
    auto insIt = std::upper_bound(headIt, headIt+numSenders, s, cmpSched);
    size_t insIdx = insIt - senders.begin();
    uint32_t hIdx = headIdx;
    uint32_t numSxAfterIns = numSenders;

    if (insIdx == hIdx) {
        if (insIdx > 0 && insIdx != s->lastIdx) {
            hIdx--;
            insIdx--;
            ASSERT(senders[hIdx] == NULL);
        }
    }

    numSxAfterIns++;
    int leftShift = (int)std::min(numSxAfterIns, (uint32_t)numToGrant) -
        (schedPrios - hIdx);

    if (leftShift > 0) {
        ASSERT(!senders[hIdx - leftShift] && (int)hIdx >= leftShift);
        hIdx -= leftShift;
        insIdx -= leftShift;
    }
    return std::make_pair(insIdx, hIdx);
}

/**
 * HomaTransport::ReceiveScheduler::UnschedRateComputer
 */
HomaTransport::ReceiveScheduler::UnschedRateComputer::UnschedRateComputer(
        HomaConfigDepot* homaConfig, bool computeAvgUnschRate,
        double minAvgTimeWindow)
    : computeAvgUnschRate(computeAvgUnschRate)
    , bytesRecvTime()
    , sumBytes(0)
    , minAvgTimeWindow(minAvgTimeWindow)
    , homaConfig(homaConfig)
{}

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
    , unschedToRecvPerPrio()
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
    , totalBytesInFlight(0)
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
    , unschedToRecvPerPrio()
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
            totalBytesInFlight = HomaPkt::getBytesOnWire(
                totalUnschedBytes, PktType::UNSCHED_DATA);
            break;
        default:
            throw cRuntimeError("Can't create inbound message "
                    "from received packet type: %d.", rxPkt->getPktType());
    }
}

void
HomaTransport::InboundMessage::fillinRxBytes(uint32_t byteStart,
        uint32_t byteEnd, PktType pktType)
{
    uint32_t bytesReceived = byteEnd - byteStart + 1;
    uint32_t bytesReceivedOnWire =
        HomaPkt::getBytesOnWire(bytesReceived, pktType);
    bytesToReceive -= bytesReceived;
    totalBytesInFlight -= bytesReceivedOnWire;
    totalBytesOnWire += bytesReceivedOnWire;

    if (pktType == PktType::SCHED_DATA) {
        ASSERT(bytesReceived > 0);
        bytesGrantedInFlight -= bytesReceived;
        rxScheduler->transport->outstandingGrantBytes -= bytesReceivedOnWire;
    }
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
    int grantedBytesOnWire =
        HomaPkt::getBytesOnWire(grantSize, PktType::SCHED_DATA);
    rxScheduler->transport->outstandingGrantBytes += grantedBytesOnWire;

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

        if (allHighPrioInflightBytes + grantedBytesOnWire >
                (int)homaConfig->maxOutstandingRecvBytes) {
            highPrioSchedDelayBytes +=
                (allHighPrioInflightBytes + grantedBytesOnWire -
                homaConfig->maxOutstandingRecvBytes);
        }
    }

    for (size_t i = 0; i <= schedPrio; i++) {
        highPrioSchedDelayBytes += (rxScheduler->scheduledBytesPerPrio[i] -
            this->scheduledBytesPerPrio[i]);
        highPrioSchedDelayBytes += (rxScheduler->unschedToRecvPerPrio[i] -
            this->unschedToRecvPerPrio[i]);
    }

    // update internal structure
    this->bytesToGrant -= grantSize;
    this->bytesGrantedInFlight += grantSize;
    this->lastGrantTime = simTime();
    this->totalBytesInFlight += grantedBytesOnWire;
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
    this->unschedToRecvPerPrio = rxScheduler->unschedToRecvPerPrio;
    this->sumInflightUnschedPerPrio = rxScheduler->getInflightUnschedPerPrio();
    this->sumInflightSchedPerPrio = rxScheduler->getInflightSchedPerPrio();
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
    this->bytesGrantedInFlight = other.bytesGrantedInFlight;
    this->totalBytesInFlight = other.totalBytesInFlight;
    this->bytesToReceive = other.bytesToReceive;
    this->msgSize = other.msgSize;
    this->totalBytesOnWire = other.totalBytesOnWire;
    this->totalUnschedBytes = other.totalUnschedBytes;
    this->msgCreationTime = other.msgCreationTime;
    this->reqArrivalTime = other.reqArrivalTime;
    this->lastGrantTime = other.lastGrantTime;
    this->highPrioSchedDelayBytes = other.highPrioSchedDelayBytes;
    this->bytesRecvdPerPrio = other.bytesRecvdPerPrio;
    this->scheduledBytesPerPrio = other.scheduledBytesPerPrio;
    this->unschedToRecvPerPrio = other.unschedToRecvPerPrio;
    this->sumInflightUnschedPerPrio = other.sumInflightUnschedPerPrio;
    this->sumInflightSchedPerPrio = other.sumInflightSchedPerPrio;
}
