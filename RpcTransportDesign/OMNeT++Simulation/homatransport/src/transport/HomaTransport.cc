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

// Required by OMNeT++ for all simple modules.
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
    //std::cout << "destructor called" << std::endl;
    delete prioResolver;
    delete distEstimator;
    delete homaConfig;
}

/**
 * This functions is used to subscribe HomaTransport's vector signals
 * and stats to the core omnet++ simulator. Vector signals are usually defined
 * for the set of signals that have a common signal name pattern with one part
 * of the name being wildcarded.
 */
void
HomaTransport::registerTemplatedStats()
{

    // Registering signals and stats for priority usage percentages.
    uint32_t numPrio = homaConfig->allPrio;
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
 * This method is a virtual method defined for every simple module in omnet++
 * simulator. OMNeT++ core simulator automatically calles this function at the
 * early stage of the simulator, after simulation objects are constructed and as
 * a part of the steps for the simlation setup.
 */
void
HomaTransport::initialize()
{
    //std::cout << "HomaTransport::initialize() invoked." << std::endl;
    // Read in config parameters for HomaTransport from config files and store
    // the parameters in a depot container.
    homaConfig = new HomaConfigDepot(this);
    //std::cout << homaConfig->destPort << std::endl;

    //std::cout << "transport ptr " << static_cast<void*>(homaConfig) << std::endl;
    // If grantMaxBytes is given too large in the config file, we should correct
    // for it.
    HomaPkt dataPkt = HomaPkt();
    dataPkt.setPktType(PktType::SCHED_DATA);
    uint32_t maxDataBytes = MAX_ETHERNET_PAYLOAD_BYTES -
        IP_HEADER_SIZE - UDP_HEADER_SIZE - dataPkt.headerSize();
    if (homaConfig->grantMaxBytes > maxDataBytes) {
        homaConfig->grantMaxBytes = maxDataBytes;
    }

    // Setting up the timer objects associated with this transport.
    sendTimer = new cMessage("SendTimer");
    emitSignalTimer = new cMessage("SignalEmitionTimer");
    nextEmitSignalTime = SIMTIME_ZERO;

    registerTemplatedStats();
    distEstimator = new WorkloadEstimator(homaConfig);
    prioResolver = new PriorityResolver(homaConfig, distEstimator);
    rxScheduler.initialize(homaConfig, prioResolver);
    sxController.initSendController(homaConfig, prioResolver);
    outstandingGrantBytes = 0;

    // set sendTimer state to START and schedule the timer at the simulation
    // start-time. When this timer fires, the rest of intializations steps will
    // be done ( ie. those step that are needed to be done after simulation
    // setup is complete.)
    sendTimer->setKind(SelfMsgKind::START);
    scheduleAt(simTime(), sendTimer);
    //std::cout << "transport ptr " << static_cast<void*>(homaConfig) << std::endl;
}

/**
 * This method performs initialization steps that are needed to be done after
 * the simulation object is created. It's only invoked when timer state is set
 * to START.
 */
void
HomaTransport::processStart()
{
    //std::cout << "processStart() init called" << std::endl;
    // initialized udp socket
    socket.setOutputGate(gate("udpOut"));
    socket.bind(homaConfig->localPort);
    sendTimer->setKind(SelfMsgKind::SEND);
    emitSignalTimer->setKind(SelfMsgKind::EMITTER);
    scheduleAt(simTime(), emitSignalTimer);
}

/**
 * This method is the single place to emit stability signal to the
 * GlobalSignalListener. At the events of a new message arrival from
 * application, transmission comletion of a message at sender, or
 * emitSignalTimer firing, this method is called and decides if it's time to
 * emit stability signal to the GlobalSignalListener. It further setup next
 * signal emission if necessary.
 */
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

/**
 * The single dispatch method for handling all messages (both self messages, ie.
 * timers, or messages from other modules) arriving at this transport. This
 * function is called by OMNeT++ main scheulder loop.
 *
 * \param msg
 *     A typical OMNeT++ message that needs to be handled. It could be either a
 *     packet arriving from network, a message from applications that needs to
 *     be transmitted or even a timer for this transport instance that needs to
 *     be handled.
 */
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
            case SelfMsgKind::BW_CHECK:
                ASSERT(msg == rxScheduler.schedBwUtilTimer);
                rxScheduler.schedSenders->handleBwUtilTimerEvent(msg);
                break;
        }
    } else {
        if (msg->arrivedOn("appIn")) {
            AppMessage* outMsg = check_and_cast<AppMessage*>(msg);
            // check and set the localAddr of this transport if this is the
            // first message arriving from applications.
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

/**
 * This method is called for packets that arrive from the network. It updates
 * the high level states and stats of the transport and dispatches the packet to
 * sender or receiver logics of the transport depending on the packet type.
 *
 * \param pkt
 *      pointer to the packet just arrived from the network.
 */
void
HomaTransport::handleRecvdPkt(cPacket* pkt)
{
    Enter_Method_Silent();
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
    // wasted bandwidth.
    if (rxScheduler.getInflightBytes() == 0) {
        // This is end of a active period, so we should dump the stats and reset
        // varibles that track next active period.
        emit(rxActiveTimeSignal, simTime() - rxScheduler.activePeriodStart);
        emit(rxActiveBytesSignal, rxScheduler.rcvdBytesPerActivePeriod);
        rxScheduler.rcvdBytesPerActivePeriod = 0;
        rxScheduler.activePeriodStart = simTime();
    }
}

/**
 * This method is called by OMNeT++ infrastructure whenever simulation is
 * ended normally or terminated.
 */
void
HomaTransport::finish()
{
    cancelAndDelete(sendTimer);
    cancelAndDelete(emitSignalTimer);
}


/**
 * Constructor for HomaTransport::SendController.
 *
 * \param transport
 *      Back pointer to the HomaTransport instance that owns this
 *      SendController.
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

/**
 * Initializing the SendController after the simulation network is setup. This
 * function is to be called by HomaTransport::intialize() method.
 *
 * \param homaConfig
 *      Homa config container that keeps user specified parameters for the
 *      transport.
 * \param prioResolver
 *      This class is used to assign priorities of unscheduled packets this
 *      sender needs to send.
 */
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

/**
 * Destructor of the HomaTransport::SendController.
 */
HomaTransport::SendController::~SendController()
{
    delete unschedByteAllocator;
    while (!outGrantQueue.empty()) {
        HomaPkt* head = outGrantQueue.top();
        outGrantQueue.pop();
        delete head;
    }

}

/**
 * This method is called after a packet is fully serialized onto the network
 * from sender's NIC. It is the place to perform the logics related to
 * the packet serialization end.
 */
void
HomaTransport::SendController::handlePktTransmitEnd()
{
    // When there are more than one packet ready for transmission and the sender
    // chooses one for transmission over the others, the sender has imposed a
    // delay in transmitting the other packets. Here we collect the statistics
    // related those trasmssion delays.
    uint32_t lastDestAddr = sentPkt.getDestAddr().toIPv4().getInt();
    for (auto it = rxAddrMsgMap.begin(); it != rxAddrMsgMap.end(); ++it) {
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

            simtime_t transmitDelay;
            if (totalTrailingUnsched > 0) {
                simtime_t totalUnschedTxTimeLeft = SimTime(1e-9 * (
                    HomaPkt::getBytesOnWire(totalTrailingUnsched,
                    PktType::UNSCHED_DATA) * 8.0 / homaConfig->nicLinkSpeed));
                transmitDelay = sentPktDuration < totalUnschedTxTimeLeft
                    ? sentPktDuration : totalUnschedTxTimeLeft;
                transport->emit(sxUnschedPktDelaySignal, transmitDelay);
            }

            if (oldestSchedPkt != currentTime) {
                transmitDelay =
                    sentPktDuration < currentTime - oldestSchedPkt
                    ? sentPktDuration : currentTime - oldestSchedPkt;
                transport->emit(sxSchedPktDelaySignal, transmitDelay);
            }
        }
    }
}

/**
 * This method handles new messages from the application that needs to be
 * transmitted over the network.
 *
 * \param sendMsg
 *      Application messsage that needs to be transmitted by the transport.
 */
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

/**
 * For every new grant that arrives from the receiver, this method is called to
 * handle that grant (ie. send scheduled packet for that grant).
 *
 * \param rxPkt
 *      receiverd grant packet from the network.
 */
void
HomaTransport::SendController::processReceivedGrant(HomaPkt* rxPkt)
{
    EV << "Received a grant from " << rxPkt->getSrcAddr().str()
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

/**
 * This method is the single interface for transmitting packets. It is called by
 * the Transport::ReceiveScheduler to send grant packets or by the
 * HomaTransport::SendController when a new data packet is ready for
 * transmission. If the NIC tx link is idle, this method sends a packet out.
 * Otherwise, to avoid buffer build up in the NIC queue, it will wait until
 * until the NIC link goes idle. This method always prioritize sending grants
 * over the data packets.
 *
 * \param msg
 *      Any of the three cases: 1. a grant packet to be transmitted out to the
 *      network. 2. The send timer that signals the NIC tx link has gone idle
 *      and we should send next ready packet.  3. NULL if a data packet has
 *      become ready and buffered in the HomaTransport::SendController queue and
 *      we want to check if we can send the packet immediately in case tx link
 *      is idle.
 */
void
HomaTransport::SendController::sendOrQueue(cMessage* msg)
{
    HomaPkt* sxPkt = NULL;
    if (msg == transport->sendTimer) {
        // Send timer fired and it's time to check if we can send a data packet.
        ASSERT(msg->getKind() == SelfMsgKind::SEND);
        if (!outGrantQueue.empty()) {
            // send queued grant packets if there is any.
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

    // When this function is called to send a grant packet.
    sxPkt = dynamic_cast<HomaPkt*>(msg);
    if (sxPkt) {
        ASSERT(sxPkt->getPktType() == PktType::GRANT);
        if (transport->sendTimer->isScheduled()) {
            // NIC tx link is busy sending another packet
            EV << "Grant timer is scheduled! Grant to " <<
                sxPkt->getDestAddr().toIPv4().str() << ", mesgId: "  <<
                sxPkt->getMsgId() << " is queued!"<< endl;

            outGrantQueue.push(sxPkt);
            return;
        } else {
            ASSERT(outGrantQueue.empty());
            EV << "Send grant to: " << sxPkt->getDestAddr().toIPv4().str()
                << ", mesgId: "  << sxPkt->getMsgId() << ", prio: " <<
                sxPkt->getGrantFields().schedPrio<< endl;
            sendPktAndScheduleNext(sxPkt);
            return;
        }
    }

    // When a data packet has become ready and we should check if we can send it
    // out.
    ASSERT(!msg);
    if (transport->sendTimer->isScheduled()) {
        return;
    }

    ASSERT(outGrantQueue.empty() && outbndMsgSet.size() == 1);
    OutboundMessage* highPrioMsg = *(outbndMsgSet.begin());
    size_t numRemoved =  outbndMsgSet.erase(highPrioMsg);
    ASSERT(numRemoved == 1); // check that the message is removed
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

/**
 * The actual interface for transmitting packets to the network. This method is
 * only called by the sendOrQueue() method.
 *
 * \param sxPkt
 *      packet to be transmitted.
 */
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

/**
 * This method cleans up a message whose tranmission is complete.
 *
 * \param msg
 *      Message that's been completely transmitted.
 */
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
 *      outbound message 1 to be compared.
 * \param msg2
 *      outbound message 2 to be compared.
 * \return
 *      true if msg1 is compared greater than msg2 (ie. pkts of msg1
 *      have higher priority for transmission than pkts of msg2)
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
 * Construcor for HomaTransport::OutboundMessage.
 *
 * \param outMsg
 *      Application message that corresponds to this OutboundMessage.
 * \param sxController
 *      Back pointer to the SendController that handles transmission of packets
 *      for every OutboundMessage in this transport..
 * \param msgId
 *      an message id to uniquely identify this OutboundMessage from every other
 *      message in this transport.
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

/**
 * Default constructor for OutboundMessage.
 */
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

/**
 * The copy constructor for this OutboundMessage.
 *
 * \param other
 *      The OutboundMessage to make a copy of.
 */
HomaTransport::OutboundMessage::OutboundMessage(const OutboundMessage& other)
{
    copy(other);
}

/**
 * Destructor for OutboundMessage.
 */
HomaTransport::OutboundMessage::~OutboundMessage()
{
    while (!txPkts.empty()) {
        HomaPkt* head = txPkts.top();
        txPkts.pop();
        delete head;
    }
}

/**
 * Assignment operator for OutboundMessage.
 *
 * \prama other
 *      Assignment OutboundMessage source.
 *
 * \return
 *      A reference to the destination OutboundMessage variable.
 */
HomaTransport::OutboundMessage&
HomaTransport::OutboundMessage::operator=(const OutboundMessage& other)
{
    if (this==&other) return *this;
    copy(other);
    return *this;
}

/**
 * The copy method
 *
 * \param other
 *      The source OutboundMessage to be copied over.
 */
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
    this->txPkts = other.txPkts;
    this->txSchedPkts = other.txSchedPkts;
}

/**
 * For every newly created OutboundMessage, calling this method will construct
 * request and unscheduled packets to be transmitted for the message and set the
 * appropriate priority for the packets.
 */
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
        /**
        EV << "Unsched pkt with msgId " << this->msgId << " ready for"
            " transmit from " << this->srcAddr.str() << " to " <<
            this->destAddr.str() << endl;
        **/

        // update the OutboundMsg for the bytes sent in this iteration of loop
        this->bytesToSched -= unschedPkt->getDataBytes();
        this->nextByteToSched += unschedPkt->getDataBytes();

        // all packet except the first one are UNSCHED
        pktType = PktType::UNSCHED_DATA;
        i++;
    } while (i < reqUnschedDataVec.size());
}

/**
 * Creates a new scheduled packet for this message and stores it in the set
 * scheduled packets ready to be transmitted for this message.
 *
 * \param numBytes
 *      number of data bytes in the scheduled packet.
 * \param schedPrio
 *      priority value assigned to this sched packet by the receiver.
 * \return
 *      remaining scheduled bytes to be sent for this message.
 */
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

    EV << "Prepared " <<  bytesToSend << " bytes for transmission from"
            << " msgId " << this->msgId << " to destination " <<
            this->destAddr.str() << " (" << this->bytesToSched <<
            " bytes left.)" << endl;
    return this->bytesToSched;
}

/**
 * Among all packets ready for transmission for this message, this function
 * retrieves highest priority packet from sender's perspective for this message.
 * Call this function only when ready to send the packet out onto the wire.
 *
 * \return outPkt
 *      The packet to be transmitted for this message
 * \return
 *      True if the returned pkt is not the last ready for transmission
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
            return (
                pkt1->getFirstByte() > pkt2->getFirstByte() ||
                (pkt1->getFirstByte() == pkt2->getFirstByte() &&
                    pkt1->getCreationTime() > pkt2->getCreationTime()) ||
                (pkt1->getFirstByte() == pkt2->getFirstByte() &&
                pkt1->getCreationTime() == pkt2->getCreationTime() &&
                pkt1->getPriority() > pkt2->getPriority()));
        default:
                throw cRuntimeError("Undefined SenderScheme parameter");
    }
}

/**
 * Constructor for HomaTransport::ReceiveScheduler.
 *
 * \param transport
 *      back pointer to the transport that owns the ReceiveScheduler.
 */
HomaTransport::ReceiveScheduler::ReceiveScheduler(HomaTransport* transport)
    : transport(transport)
    , homaConfig(NULL)
    , unschRateComp(NULL)
    , ipSendersMap()
    , grantTimersMap()
    , schedSenders(NULL)
    , schedBwUtilTimer(NULL)
    , bwCheckInterval(0)
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

/**
 * This method is to perform setup steps ReceiveScheduler that are required to
 * be done after the simulation network is setup. This should be called from
 * the HomaTransport::initialize() function.
 *
 * \param homaConfig
 *      Collection of all user specified parameters for the transport.
 * \param prioResolver
 *      Transport instance of PriorityResolver that is used for determining
 *      scheduled packet priority that is specified in the grants that this
 *      ReceiveScheduler sends.
 */
void
HomaTransport::ReceiveScheduler::initialize(HomaConfigDepot* homaConfig,
    PriorityResolver* prioResolver)
{
    this->homaConfig = homaConfig;
    this->unschRateComp = new UnschedRateComputer(homaConfig,
        homaConfig->useUnschRateInScheduler, 0.1);
    this->schedSenders = new SchedSenders(homaConfig, transport, this);
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
    schedBwUtilTimer = new cMessage("bwUtilChecker");
    schedBwUtilTimer->setKind(SelfMsgKind::BW_CHECK);
    bwCheckInterval = SimTime(homaConfig->linkCheckBytes * 8.0 /
        (homaConfig->nicLinkSpeed * 1e9));
}

/**
 * ReceiveScheduler destructor.
 */
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
                itt != s->incompleteMesgs.end(); ++itt) {
            InboundMessage* mesg = itt->second;
            delete mesg;
        }
        delete s;
    }
    delete unschRateComp;
    delete schedSenders;
}

/**
 * Given a data packet just received from the network, this method finds the
 * associated receiving message that this packet belongs to.
 *
 * \param rxPkt
 *      Received data packet for which this function finds the corresponding
 *      inbound message.
 * \return
 *      The inbound message that rxPkt is sent for from the sender or NULL if
 *      no such InboundMessage exists (ie. this is the first received packet of
 *      the message.)
 */
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
 * This method is to handle data packets arriving at the receiver's transport.
 *
 * \param rxPkts
 *      Received data packet (ie. REQUEST, UNSCHED_DATA, or SCHED_DATA).
 */
void
HomaTransport::ReceiveScheduler::processReceivedPkt(HomaPkt* rxPkt)
{
    uint32_t pktLenOnWire = HomaPkt::getBytesOnWire(rxPkt->getDataBytes(),
        (PktType)rxPkt->getPktType());
    simtime_t pktDuration =
        SimTime(1e-9 * (pktLenOnWire * 8.0 / homaConfig->nicLinkSpeed));
    simtime_t timeNow = simTime();

    // check and update states for oversubscription time and bytes.
    if (schedSenders->numSenders > schedSenders->numToGrant) {
        // Already in an oversubcription period
        ASSERT(inOversubPeriod && oversubPeriodStop==MAXTIME &&
            oversubPeriodStart < timeNow - pktDuration);
        rcvdBytesPerOversubPeriod += pktLenOnWire;
    } else if (oversubPeriodStop != MAXTIME) {
        // an oversubscription period has recently been marked ended and we need
        // to emit signal to record the stats.
        ASSERT(!inOversubPeriod && oversubPeriodStop <= timeNow);
        simtime_t oversubDuration = oversubPeriodStop-oversubPeriodStart;
        simtime_t oversubOffset = oversubPeriodStop - timeNow + pktDuration;
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
    // can change. So, remove s from the schedSender, create a SchedState for s,
    // handle the packet.
    SchedSenders::SchedState old;
    old.setVar(schedSenders->numToGrant, schedSenders->headIdx,
        schedSenders->numSenders, s, 0);
    int sIndOld = schedSenders->remove(s);
    old.sInd = sIndOld;
    /**
    EV << "Remove sender at ind " << sIndOld << " and handled pkt" << endl;
    **/
    EV << "\n\n#################New packet arrived################\n\n" << endl;

    // process received packet
    auto msgCompHandle = s->handleInboundPkt(rxPkt);

    // Handling a received packet can change the state of the scheduled senders.
    // We need to check for message completion and see if new grant should be
    // transmitted.
    SchedSenders::SchedState cur;
    auto ret = schedSenders->insPoint(s);
    cur.setVar(schedSenders->numToGrant, std::get<1>(ret), std::get<2>(ret),
        s, std::get<0>(ret));

    EV << "Pkt arrived, SchedState before handling pkt:\n\t" << old << endl;
    EV << "SchedState after handling pkt:\n\t" << cur << endl;

    schedSenders->handleMesgRecvCompletionEvent(msgCompHandle, old, cur);

    EV << "SchedState before mesgCompletHandler:\n\t" << old << endl;
    EV << "SchedState after mesgCompletHandler:\n\t" << cur << endl;

    schedSenders->handlePktArrivalEvent(old, cur);
    EV << "SchedState before handlePktArrival:\n\t" << old << endl;
    EV << "SchedState after handlePktArrival:\n\t" << cur << endl;

    // check and update states for oversubscription time and bytes.
    if (schedSenders->numSenders <= schedSenders->numToGrant) {
        if (inOversubPeriod) {
            // oversubscription period ended. emit signals and record stats.
            transport->emit(oversubTimeSignal, timeNow - oversubPeriodStart);
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
        oversubPeriodStart = timeNow - pktDuration;
        oversubPeriodStop = MAXTIME;
        rcvdBytesPerOversubPeriod += pktLenOnWire;

    }
    delete rxPkt;

    // Each new data packet arrival is a hint that recieve link is being
    // utilized and we need to cancel/reset the schedBwUtilTimer. The code block
    // below does this job.
    if (!schedSenders->numSenders) {
        // If no sender is waiting for grants, then there's no point to
        // track sched bw utilization.
        transport->cancelEvent(schedBwUtilTimer);
        return;
    }

    if (!schedBwUtilTimer->isScheduled()) {
        transport->scheduleAt(timeNow + bwCheckInterval,
            schedBwUtilTimer);
        EV << "Scheduled schedBwUtilTimer at " <<
            schedBwUtilTimer->getArrivalTime() << endl;
        return;
    }

    simtime_t schedTime = schedBwUtilTimer->getArrivalTime();
    transport->cancelEvent(schedBwUtilTimer);
    transport->scheduleAt(std::max(schedTime, timeNow + bwCheckInterval),
        schedBwUtilTimer);
    EV << "scheduled schedBwUtilTimer at " <<
            schedBwUtilTimer->getArrivalTime() << endl;
    return;
}

/**
 * This method handles pacing grant transmissions. Every time a grant is sent
 * for a sender, a grant timer specific to that sender is set to signal the next
 * grant (if needed) at one packet serialization time later. This method
 * dispatches next grant timer to the right message.
 *
 * \param grantTimer
 *      Grant pacer that just fired and is to be handled.
 */
void
HomaTransport::ReceiveScheduler::processGrantTimers(cMessage* grantTimer)
{
    EV << "\n\n################Process Grant Timer################\n\n" << endl;
    SenderState* s = grantTimersMap[grantTimer];
    schedSenders->handleGrantTimerEvent(s);

    //schedSenders->remove(s);
    //auto ret = schedSenders->insPoint(s);
    //int sInd = ret.first;
    //int headInd = ret.second;
    //schedSenders->handleGrantRequest(s, sInd, headInd);
    if (inOversubPeriod &&
            schedSenders->numSenders <= schedSenders->numToGrant) {
        // Receiver was in an oversubscriped period which is now ended after
        // sending the most recent grant. Mark the end of oversubscription
        // period. Later we dump the statistics in the next packet arrival
        // event.
        inOversubPeriod = false;
        oversubPeriodStop = simTime();
    }
}

/**
 * For every packet that arrives, this function is called to update
 * stats-tracking variables in ReceiveScheduler.
 *
 * \param pktType
 *      Which kind of packet has arrived: REQUEST, UNSCHED_DATA, SCHED_DATA,
 *      GRANT.
 * \param prio
 *      priority of the received packet.
 * \param dataBytesInPkt
 *      lenght of the data portion of the packet in bytes.
 */
void
HomaTransport::ReceiveScheduler::addArrivedBytes(PktType pktType, uint16_t prio,
    uint32_t dataBytesInPkt)
{
    uint32_t arrivedBytesOnWire =
        HomaPkt::getBytesOnWire(dataBytesInPkt, pktType);
    allBytesRecvd += arrivedBytesOnWire;
    bytesRecvdPerPrio.at(prio) += arrivedBytesOnWire;
}

/**
 * For every grant packet sent to the sender, this function is called to update
 * the variables tracking statistics fo outstanding grants, scheduled packets,
 * and bytes.
 *
 * \param prio
 *      Scheduled packet priority specified in the grant packet.
 * \param grantedBytes
 *      Scheduled bytes granted in this the grant packet.
 */
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

/**
 * For every message that arrives at the receiver, this function is called to
 * update the variables tracking the statistics of inflight unscheduled
 * bytes to arrive.
 *
 * \param pktType
 *      The king of the packet unscheduled data will arrive in. Either REQUEST
 *      or UNSCHED_DATA.
 * \param prio
 *      Priority of the packet the unscheduled data arrive in.
 * \param bytesToArrive
 *      The size of unscheduled data in the packet carrying it.
 */
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


/**
 * For every packet that we have expected to arrive and called either of
 * addPendingUnschedBytes() or addPendingGrantedBytes() methods for them, we
 * call this function to at the reception of the packet at receiver. Call to
 * this function updates the variables that track statistics of outstanding
 * bytes.
 *
 * \param pktType
 *      Kind of the packet that has arrived.
 * \param prio
 *      Priority of the arrived packet.
 * \param dataBytesInPkt
 *      Length of the data delivered in the packet.
 */
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
 * Constructor of HomaTransport::ReceiveScheduler::SenderState.
 *
 * \param srcAddr
 *      Address of the sender corresponding to this SenderState.
 * \param rxScheduler
 *      The ReceiveScheduler that handles received packets for this SenderState.
 * \param grantTimer
 *      The timer that paces grants for the sender corresponding to this
 *      SenderState.
 * \param homaConfig
 *      Collection of user-specified config parameters for this transport
 */
HomaTransport::ReceiveScheduler::SenderState::SenderState(
        inet::IPv4Address srcAddr, ReceiveScheduler* rxScheduler,
        cMessage* grantTimer, HomaConfigDepot* homaConfig)
    : homaConfig(homaConfig)
    , rxScheduler(rxScheduler)
    , senderAddr(srcAddr)
    , grantTimer(grantTimer)
    , timePaceGrants(true)
    , mesgsToGrant()
    , incompleteMesgs()
    , lastGrantPrio(homaConfig->allPrio)
    , lastIdx(homaConfig->adaptiveSchedPrioLevels)
{}

/**
 * Compute a lower bound for the next time that the receiver can send a grant
 * for the sender of this SenderState.
 *
 * \param currentTime
 *      Time at which the most recent grant packet is sent for the center.
 * \param grantSize
 *      Number of bytes granted in the most recent grant sent.
 */
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
 * This method handles packets received from the sender corresponding to
 * SenderState. It finds the sender's InboundMessage that this belongs to and
 * invoked InboundMessage methods to fills in the delivered packet data in the
 * message.
 *
 * \param rxPkt
 *      The data packet received from the sender.
 * \return
 *      returns a pair <scheduledMesg?, incompletMesg?>.
 *      schedMesg is True if rxPkt belongs to a scheduled message (ie. a message
 *      that needs at least one grant when the receiver first know about it).
 *
 *      schedMesg is False if rxPkt belongs to a fully unscheduled message (a
 *      message that all of its bytes are received in unscheduled packets).
 *
 *      incompleteMesg is -1 if rxPkt is the last packet of an inbound message and
 *      reception of the rxPkt completes the reception of the message.
 *
 *      incompleteMesg is 0 if scheduledMesg is True and the mesg rxPkt belongs to
 *      is not complete (ie. has more inbound inflight packets) but it doesn't
 *      any more grants.
 *
 *      incompleteMesg is a positive rank number if scheduledMesg is True and
 *      the mesg rxPkt belongs to needs more grants. In which case the positive
 *      rank number is 1 if the mesg is the most prefered mesg among all
 *      scheduled message of the sender. Otherwise, rank number is 2.
 *
 *      incompleteMesg is the positive remaining bytes of the mesg yet to be
 *      received if scheduledMesg is False.
 */
std::pair<bool, int>
HomaTransport::ReceiveScheduler::SenderState::handleInboundPkt(HomaPkt* rxPkt)
{
    std::pair<bool, int> ret;
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

    bool isMesgSched =
        (inboundMesg->msgSize - inboundMesg->totalUnschedBytes > 0);
    ret.first = isMesgSched;
    if (!isMesgSched) {
        ASSERT(inboundMesg->msgSize == inboundMesg->totalUnschedBytes);
    }

    if (inboundMesg->bytesToReceive == 0) {
        ASSERT(inboundMesg->bytesToGrant == 0);

        // this message is complete, so send it to the application
        AppMessage* rxMsg = inboundMesg->prepareRxMsgForApp();

#if TESTING
        delete rxMsg;
#else
        rxScheduler->transport->send(rxMsg, "appOut", 0);
#endif

        // remove this message from the incompleteRxMsgs
        incompleteMesgs.erase(inboundMesg->msgIdAtSender);
        delete inboundMesg;
        ret.second = -1;
    } else if (inboundMesg->bytesToGrant == 0) {
        // no grants needed for this mesg but mesg is not complete yet and we
        // need to keep the message aroudn until all of its packets are arrived.
        if (isMesgSched) {
            ret.second = 0;
        } else {
            ret.second = inboundMesg->bytesToReceive;
        }
    } else {
        auto resPair = mesgsToGrant.insert(inboundMesg);
        ASSERT(resPair.second);
        ret.second = (resPair.first == mesgsToGrant.begin()) ? 1 : 2;
    }
    return ret;
}

/**
 * ReceiveScheduler calls this method to send a grant for a sender's highest
 * priority message of this receiver. As a side effect, this method also
 * schedules the next grant for that sender at one grant serialization time
 * later in the future. This time is a lower bound for next grant time and
 * ReceiveScheduler will decide if next grant should be sent at that time or
 * not.
 *
 * \param grantPrio
 *      Priority of the scheduled packet that sender will send for this grant.
 * \return
 *      1) Return 0 if a grant cannot be sent (ie. the outstanding bytes for this
 *      sender are more than one RTTBytes or a grant timer is scheduled for the
 *      future.). 2) Sends a grant and returns remaining bytes to grant for the
 *      most preferred message of the sender. 3) Sends a grant and returns -1 if
 *      the sender has no more outstanding messages that need grants.
 */
int
HomaTransport::ReceiveScheduler::SenderState::sendAndScheduleGrant(
        uint32_t grantPrio)
{
    simtime_t currentTime = simTime();
    auto topMesgIt = mesgsToGrant.begin();
    ASSERT(topMesgIt != mesgsToGrant.end());
    InboundMessage* topMesg = *topMesgIt;
    ASSERT(topMesg->bytesToGrant > 0);

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

    // update stats and send a grant
    grantSize = grantPkt->getGrantFields().grantBytes;
    uint16_t prio = grantPkt->getGrantFields().schedPrio;
    rxScheduler->addPendingGrantedBytes(prio, grantSize);
    topMesg->updatePerPrioStats();
    rxScheduler->transport->sxController.sendOrQueue(grantPkt);
    rxScheduler->transport->emit(outstandingGrantBytesSignal,
        rxScheduler->transport->outstandingGrantBytes);
    if (topMesg->bytesToGrant > 0) {
        if (topMesg->totalBytesInFlight < homaConfig->maxOutstandingRecvBytes &&
                timePaceGrants) {
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

    if (newTopMesg->totalBytesInFlight < homaConfig->maxOutstandingRecvBytes &&
            timePaceGrants) {
        rxScheduler->transport->scheduleAt(
            getNextGrantTime(simTime(), grantSize), grantTimer);
    }
    return newTopMesg->bytesToGrant;
}

/**
 * Constructor for HomaTransport::ReceiveScheduler::SchedSenders.
 *
 * \param homaConfig
 *      Collection of all user specified config parameters for the transport
 */
HomaTransport::ReceiveScheduler::SchedSenders::SchedSenders(
        HomaConfigDepot* homaConfig, HomaTransport* transport,
        ReceiveScheduler* rxScheduler)
    : transport(transport)
    , senders()
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
 * This function implements the logics of the receiver scheduler for sending
 * grants to a sender. At every event such as packet arrival or grant
 * transmission, that a sender's ranking changes in the view of the receiver,
 * this method is called to check if a grant should be sent for that sender and
 * this function then sends the grant if needed. N.B. the sender must have been
 * removed from schedSenders prior to the call to this method. The side effect
 * of this method invokation is that it also inserts the SenderState into the
 * schedSenders list if it needs more grants.
 *
 * \param s
 *      SenderState corresponding to the sender for which we want to test and
 *      send a grant.
 * \param sInd
 *      The position of the sender in the senders list which determines the
 *      ranking of the sender in the receiver's eye and if a grant should be
 *      sent.
 * \param headInd
 *      The index of the top rank or most preferred sender in the receiver's
 *      senders list.
 */
/*
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

    // The grant sent, was the last grant of the 1st preferred sched message of
    // s. The 2nd preffered sched message of s, ranks s beyond number of senders
    // we can grant.
    if (sIndNew > 0) {
        insert(s);
    }
    int indToGrant = headIdx + numToGrant - 1;
    s = removeAt(indToGrant);
    if (!s)
        return;

    auto retNew = insPoint(s);
    int indToGrantNew = retNew.first;
    int headIndNewNew = retNew.second;
    handleGrantRequest(s, indToGrantNew, headIndNewNew);
}
*/

/**
 * Removes a SenderState from sendrs list.
 *
 * \param s
 *      SenderState to be removed.
 * \return
 *      Negative value means the mesg didn't belong to the schedSenders so can't
 *      be removed. Otherwise, it returns the index at which s was removed from.
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
        s->lastIdx = ind;
        return ind;
    } else {
        throw cRuntimeError("Trying to remove an unlisted sched sender!");
    }
}

/**
 * Remove a SenderState at a specified index in senders list.
 *
 * \param rmIdx
 *      Index of SenderState instace in senders list that we want to remove.
 * \return
 *      Null if no element exists at rmIdx. Otherwise, it return the removed
 *      element.
 */
HomaTransport::ReceiveScheduler::SenderState*
HomaTransport::ReceiveScheduler::SchedSenders::removeAt(uint32_t rmIdx)
{
    if (rmIdx < headIdx || rmIdx >= (headIdx + numSenders)) {
        EV << "Objects are within range [" << headIdx << ", " <<
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
 * Insert a SenderState into the senders list. The caller must make sure that the
 * SenderState is not already in the list (ie. by calling remove) and s infact
 * belong to the senders list (ie. sender of s has messages that need grant).
 * Element s then will be inserted at the position insId returned from insPoint
 * method and headIdx will be updated.
 *
 * \param s
 *      SenderState to be inserted in the senders list.
 */
void
HomaTransport::ReceiveScheduler::SchedSenders::insert(SenderState* s)
{
    ASSERT(!s->mesgsToGrant.empty());
    auto ins = insPoint(s);
    size_t insIdx = std::get<0>(ins);
    size_t newHeadIdx = std::get<1>(ins);

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
 * Given a SenderState s, this method find the index of senders list at which s
 * should be inserted. The caller should make sure that s is not already in the
 * list (ie. by calling remove()).
 *
 * \param s
 *      SenderState to be inserted.
 * \return
 *      returns a tuple of (insertedIndex, headIndex, numSenders) as if s is
 *      inserted in the list. insertedIndex negative, means s doesn't belong to
 *      the schedSenders.
 */
std::tuple<int, int, int>
HomaTransport::ReceiveScheduler::SchedSenders::insPoint(SenderState* s)
{
    CompSched cmpSched;
    if (s->mesgsToGrant.empty()) {
        return std::make_tuple(-1, headIdx, numSenders);
    }
    std::vector<SenderState*>::iterator headIt = headIdx + senders.begin();
    auto insIt = std::upper_bound(headIt, headIt+numSenders, s, cmpSched);
    size_t insIdx = insIt - senders.begin();
    uint32_t hIdx = headIdx;
    uint32_t numSxAfterIns = numSenders;

    EV << "insIdx: " << insIdx << ", hIdx: " << hIdx << ", last index: " << s->lastIdx << endl;
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
    EV << "insIdx: " << insIdx << ", hIdx: " << hIdx << ", Left shift: " << leftShift << endl;
    if (leftShift > 0) {
        ASSERT(!senders[hIdx - leftShift] && (int)hIdx >= leftShift);
        hIdx -= leftShift;
        insIdx -= leftShift;
    }
    return std::make_tuple(insIdx, hIdx, numSxAfterIns);
}

/**
 * Processes the schedBwUtilTimer triggers and implements the mechanism to
 * properly react to wasted receiver bandwidth when scheduled senders don't send
 * scheduled packets in a timely fashion.
 *
 * \param timer
 *      Timer object that triggers when bw wastage is detected (ie. the receiver
 *      expected scheduled packets but they weren't received as expected.)
 */
void
HomaTransport::ReceiveScheduler::SchedSenders::handleBwUtilTimerEvent(
        cMessage* timer) {

    EV << "\n\n###############Process BwUtil Timer################\n\n" << endl;
    simtime_t timeNow = simTime();
    if (numSenders < numToGrant) {
        return;
    }

    int indToGrant = headIdx + numToGrant - 1;
    SenderState* lowPrioSx = senders[indToGrant];

    // Runtime testing with asserts
    auto topMesgIt = lowPrioSx->mesgsToGrant.begin();
    ASSERT(topMesgIt != lowPrioSx->mesgsToGrant.end());
    InboundMessage* topMesg = *topMesgIt;
    ASSERT(topMesg->bytesToGrant > 0);
    // End testing

    SchedState ss;
    if (!lowPrioSx->timePaceGrants) {
        // we have previously incremented overcommittment level
        ss.setVar(numToGrant, headIdx, numSenders, lowPrioSx, indToGrant);
        lowPrioSx->sendAndScheduleGrant(getPrioForMesg(ss));

        if (topMesg->totalBytesInFlight >= homaConfig->maxOutstandingRecvBytes) {
            lowPrioSx->timePaceGrants = true;
        }

        // enable timer for next bubble detection
        transport->scheduleAt(timeNow + rxScheduler->bwCheckInterval,
            rxScheduler->schedBwUtilTimer);
        return;
    }

    // Bubble detected and no sender has been previously promoted, so promote
    // next ungranted sched sender and send grants for it.
    if (numSenders <= numToGrant) {
        // all sched senders are currently being granted. No need to increase
        // numToGrant
        return;
    }
    numToGrant++;
    if (headIdx) {
        // Runtime testing with asserts
        for (size_t i = 0; i < headIdx; i++) {
            ASSERT(!senders[i]);
        } // End testing
        headIdx--;
        senders.erase(senders.begin());
    }

    // enable timer for next bubble detection
    transport->scheduleAt(timeNow + rxScheduler->bwCheckInterval,
        rxScheduler->schedBwUtilTimer);

    indToGrant = headIdx + numToGrant - 1;
    lowPrioSx = senders[indToGrant];

    // Runtime testing with asserts
    topMesgIt = lowPrioSx->mesgsToGrant.begin();
    ASSERT(topMesgIt != lowPrioSx->mesgsToGrant.end());
    topMesg = *topMesgIt;
    ASSERT(topMesg->bytesToGrant > 0);
    ASSERT(lowPrioSx->timePaceGrants);
    // End testing

    lowPrioSx->timePaceGrants = false;
    ss.setVar(numToGrant, headIdx, numSenders, lowPrioSx, indToGrant);
    lowPrioSx->sendAndScheduleGrant(getPrioForMesg(ss));
}


void
HomaTransport::ReceiveScheduler::SchedSenders::handlePktArrivalEvent(
    SchedState& old, SchedState& cur)
{
    if (cur.sInd < 0) {

        if (old.sInd < 0 || old.sInd >= (old.numToGrant + old.headIdx)) {
            return;
        }

        if (old.sInd >= old.headIdx &&
                old.sInd < old.numToGrant + old.headIdx) {
            int indToGrant;
            SenderState* sToGrant;
            if (cur.numSenders < cur.numToGrant) {
                indToGrant = cur.headIdx + cur.numSenders - 1;
                sToGrant = removeAt(indToGrant);
            } else {
                indToGrant = cur.headIdx + cur.numToGrant - 1;
                sToGrant = removeAt(indToGrant);
            }

            if (sToGrant->timePaceGrants) {
                old = cur;
                old.s = sToGrant;
                old.sInd = indToGrant;

                sToGrant->sendAndScheduleGrant(getPrioForMesg(old));

                auto ret = insPoint(sToGrant);
                cur.numToGrant = numToGrant;
                cur.sInd = std::get<0>(ret);
                cur.headIdx = std::get<1>(ret);
                cur.numSenders = std::get<2>(ret);
                cur.s = sToGrant;
                handleGrantSentEvent(old, cur);
            }

            return;
        }
        throw cRuntimeError("invalid old position for sender in the receiver's"
            " preference list");
    }

    if (cur.sInd >= cur.headIdx + cur.numToGrant) {
        if (old.sInd < 0 || old.sInd >= old.headIdx + old.numToGrant) {
            insert(cur.s);
            return;
        }

        if (old.sInd >= old.headIdx && old.sInd < old.numToGrant + old.headIdx) {
            insert(cur.s);
            int indToGrant = cur.headIdx + cur.numToGrant - 1;
            SenderState* sToGrant = removeAt(indToGrant);

            if (sToGrant->timePaceGrants) {
                old = cur;
                old.s = sToGrant;
                old.sInd = indToGrant;

                sToGrant->sendAndScheduleGrant(getPrioForMesg(old));

                auto ret = insPoint(sToGrant);
                cur.numToGrant = numToGrant;
                cur.sInd = std::get<0>(ret);
                cur.headIdx = std::get<1>(ret);
                cur.numSenders = std::get<2>(ret);
                cur.s = sToGrant;
                handleGrantSentEvent(old, cur);
            }
            return;
        }
        throw cRuntimeError("invalid old position for sender in the receiver's"
            " preference list");
    }

    if (cur.sInd < cur.headIdx + cur.numToGrant) {
        if (old.sInd < 0 || old.sInd >= old.numToGrant + old.sInd) {
            if (cur.numSenders > cur.numToGrant) {
                if (cur.sInd == cur.headIdx + cur.numToGrant - 1) {
                    cur.s->timePaceGrants = senders[cur.sInd]->timePaceGrants;
                    senders[cur.sInd]->timePaceGrants = true;
                } else {
                    auto premtdSndr1 = senders[cur.headIdx + cur.numToGrant - 1];
                    auto premtdSndr2 = senders[cur.headIdx + cur.numToGrant - 2];
                    premtdSndr2->timePaceGrants = premtdSndr1->timePaceGrants;
                    premtdSndr1->timePaceGrants = true;
                }
            }
        } else if (cur.numSenders >= cur.numToGrant) {
            if (cur.sInd == cur.headIdx + cur.numToGrant - 1 &&
                    old.sInd < old.headIdx + old.numToGrant -1) {
                cur.s->timePaceGrants = senders[cur.sInd-1]->timePaceGrants;
                senders[cur.sInd-1]->timePaceGrants = true;
            } else if (old.sInd == old.headIdx + old.numToGrant - 1 &&
                    cur.sInd < cur.headIdx + cur.numToGrant -1) {
                senders[cur.sInd-1]->timePaceGrants = cur.s->timePaceGrants;
                cur.s->timePaceGrants = true;
            }
        }

        if (cur.s->timePaceGrants) {
            old = cur;
            cur.s->sendAndScheduleGrant(getPrioForMesg(old));
            auto ret = insPoint(cur.s);
            cur.sInd = std::get<0>(ret);
            cur.headIdx = std::get<1>(ret);
            cur.numSenders = std::get<2>(ret);
            handleGrantSentEvent(old, cur);
        }
        return;
    }

    ASSERT(cur.sInd >= 0 && cur.sInd < cur.headIdx);
    throw cRuntimeError("Error, sender has invalid position in receiver's "
        "preference list.");
}

void
HomaTransport::ReceiveScheduler::SchedSenders::handleGrantSentEvent(
    SchedState& old, SchedState& cur)
{


    ASSERT(old.sInd >= old.headIdx && old.sInd < old.headIdx + old.numToGrant);
    if (old.sInd == cur.sInd) {
        ASSERT (cur.sInd < cur.headIdx + cur.numToGrant);
        insert(cur.s);

        EV << "SchedState before handleGrantSent:\n\t" << old << endl;
        EV << "SchedState after handleGrantSent:\n\t" << cur << endl;
        return;
    }

    if (cur.sInd >= 0 && cur.sInd < cur.headIdx) {
        throw cRuntimeError("Error, sender has invalid position in receiver's "
            "preference list.");
    }

    if (cur.sInd < 0 || cur.sInd >= cur.numToGrant + cur.headIdx) {
        if (old.numSenders > old.numToGrant) {
            int ind = cur.headIdx + cur.numToGrant - 1;
            if (old.sInd == old.headIdx + old.numToGrant - 1) {
                senders[ind]->timePaceGrants = cur.s->timePaceGrants;
            } else {
                senders[ind]->timePaceGrants = senders[ind-1]->timePaceGrants;
                senders[ind-1]->timePaceGrants = true;
            }
        }
        cur.s->timePaceGrants = true;


    } else {
        if (old.sInd == old.numToGrant + old.headIdx - 1 &&
                !old.s->timePaceGrants) {
            senders[cur.headIdx + cur.numToGrant - 2]->timePaceGrants = false;
            cur.s->timePaceGrants = true;
        } else if (cur.sInd == cur.numToGrant + cur.headIdx - 1 &&
                !senders[cur.numToGrant + cur.headIdx - 2]->timePaceGrants) {
            cur.s->timePaceGrants = false;
            senders[cur.numToGrant + cur.headIdx - 2]->timePaceGrants = true;
        }
    }

    if (cur.sInd >= 0) {
        insert(cur.s);
    }

    if ((cur.sInd < 0 || cur.sInd >= cur.numToGrant + cur.headIdx) &&
            cur.numSenders >= cur.numToGrant) {
        int indToGrant = cur.numToGrant + cur.headIdx - 1;
        SenderState* sToGrant = removeAt(indToGrant);
        if (sToGrant->timePaceGrants) {
            old = cur;
            old.s = sToGrant;
            old.sInd = indToGrant;

            sToGrant->sendAndScheduleGrant(getPrioForMesg(old));

            auto ret = insPoint(sToGrant);
            cur.numToGrant = numToGrant;
            cur.sInd = std::get<0>(ret);
            cur.headIdx = std::get<1>(ret);
            cur.numSenders = std::get<2>(ret);
            cur.s = sToGrant;
            handleGrantSentEvent(old, cur);
        }
    }
    EV << "SchedState before handleGrantSent:\n\t" << cur << endl;
    EV << "SchedState after handleGrantSent:\n\t" << cur << endl;
    return;
}

void
HomaTransport::ReceiveScheduler::SchedSenders::handleGrantTimerEvent(
    SenderState* s)
{
    remove(s);
    auto ret = insPoint(s);
    int sInd = std::get<0>(ret);
    int headInd = std::get<1>(ret);
    int sendersNum = std::get<2>(ret);

    if (sInd < 0)
        return;

    if (sInd - headInd >= numToGrant) {
        insert(s);
        return;
    }

    SchedState old;
    SchedState cur;
    if (s->timePaceGrants) {
        old.numToGrant = numToGrant;
        old.headIdx = headInd;
        old.numSenders = sendersNum;
        old.s = s;
        old.sInd = sInd;

        s->sendAndScheduleGrant(getPrioForMesg(old));

        auto ret = insPoint(s);
        cur.numToGrant = numToGrant;
        cur.s = s;
        cur.sInd = std::get<0>(ret);
        cur.headIdx = std::get<1>(ret);
        cur.numSenders = std::get<2>(ret);
        handleGrantSentEvent(old, cur);
        return;
    }
}

void
HomaTransport::ReceiveScheduler::SchedSenders::handleMesgRecvCompletionEvent(
    const std::pair<bool, int>& msgCompHandle, SchedState& old, SchedState& cur)
{
    bool schedMesgFin = msgCompHandle.first;
    int mesgRemain = msgCompHandle.second;
    if (!schedMesgFin || mesgRemain >= 0) {
        // if a scheduled mesg is not completed before the call to this method,
        // there's nothing to do here; just return;
        return;
    }

    ASSERT(cur.numToGrant == numToGrant && old.numToGrant == numToGrant);
    if (numToGrant == homaConfig->numSendersToKeepGranted) {
        // if numToGrant is at its lowest possible value, there's nothing to do
        // here; just return;
        return;
    }

    if (cur.sInd < 0) {
        // s is not a schedSender
        cur.s->timePaceGrants = true;
        if (old.sInd < 0 ) {
            // s either has been fully granted in the past or not eligible for
            // grants, and one of its granted mesgs is completed.
            if (cur.numSenders > numToGrant) {
                auto premtdSndr = senders[cur.headIdx + cur.numToGrant - 1];
                premtdSndr->timePaceGrants = true;
            }
            numToGrant--;
            cur.numToGrant--;
            return;

        } else if (old.sInd >= old.headIdx + old.numToGrant - 1) {
            if (cur.numSenders > numToGrant) {
                auto premtdSndr = senders[cur.headIdx + cur.numToGrant - 1];
                premtdSndr->timePaceGrants = true;
            }

            numToGrant--;
            cur.numToGrant--;
            return;
        } else {
            // s was among preferred mesgs for granting, but not any more
            numToGrant--;
            cur.numToGrant--;
            if (cur.numSenders >= cur.numToGrant) {
                auto premtdSndr = senders[cur.headIdx + cur.numToGrant - 1];
                premtdSndr->timePaceGrants = true;
            }
            return;
        }

    } else if (cur.sInd >= cur.numToGrant + cur.headIdx - 1) {
        cur.s->timePaceGrants = true;
        if (old.sInd >= old.numToGrant + old.headIdx - 1 || old.sInd < 0) {
            if (cur.numSenders > cur.numToGrant) {
                auto premtdSndr = senders[cur.headIdx + cur.numToGrant - 1];
                premtdSndr->timePaceGrants = true;
            }
            numToGrant--;
            cur.numToGrant--;
            return;
        } else {
            // s was among preferred mesgs for granting, but not any more
            numToGrant--;
            cur.numToGrant--;
            if (cur.numSenders >= cur.numToGrant) {
                auto premtdSndr = senders[cur.headIdx + cur.numToGrant - 1];
                premtdSndr->timePaceGrants = true;
            }
            return;
        }
    } else {
        // s is among the senders to be granted
        cur.s->timePaceGrants = true;
        if (old.sInd < 0 || old.sInd >= old.numToGrant + old.headIdx - 1) {
            if (cur.numSenders > cur.numToGrant) {
                auto premtdSndr = senders[old.headIdx + old.numToGrant - 1];
                premtdSndr->timePaceGrants = true;
            }
            numToGrant--;
            cur.numToGrant--;
            return;
        } else {
            numToGrant--;
            cur.numToGrant--;
            if (cur.numSenders > cur.numToGrant) {
                int premtdInd;
                if (old.sInd == old.headIdx) {
                    ASSERT(old.headIdx == cur.headIdx);
                    premtdInd = old.headIdx + cur.numToGrant;
                } else {
                    premtdInd = old.headIdx + cur.numToGrant - 1;
                }
                auto premtdSndr = senders[premtdInd];
                premtdSndr->timePaceGrants = true;
            }
            return;
        }
    }
}

uint16_t
HomaTransport::ReceiveScheduler::SchedSenders::getPrioForMesg(SchedState& st)
{
    int grantPrio =
        st.sInd + homaConfig->allPrio - homaConfig->adaptiveSchedPrioLevels;
    grantPrio = std::min(homaConfig->allPrio - 1, grantPrio);
    EV << "Get prio for mesg. sInd: " << st.sInd << ", grantPrio: " <<
        grantPrio << ", allPrio: " << homaConfig->allPrio << ", schedPrios: " <<
        homaConfig->adaptiveSchedPrioLevels << endl;

    return grantPrio;
}

/**
 * Constructor of HomaTransport::ReceiveScheduler::UnschedRateComputer.
 *
 * \param homaConfig
 *      Collection of user specified config parameters for the transport.
 * \param computeAvgUnschRate
 *      True enables this modules.
 * \param minAvgTimeWindow
 *      Time interval over which this module computes the average unscheduled
 *      rate.
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
 * Interface to receive computed average unscheduled bytes rate.
 *
 * \return
 *      Returns the fraction of bw used by the unsched (eg. average unsched rate
 *      divided by nic link speed.)
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

/**
 * Interface for accumulating the unsched bytes for unsched rate calculation.
 * This is called evertime an unscheduled packet arrives.
 *
 * \param arrivalTime
 *      Time at which unscheduled packet arrived.
 * \param bytesRecvd
 *      Total unsched bytes received including all headers and packet overhead
 *      bytes on wire.
 */
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
 * Default Constructor of HomaTransport::InboundMessage.
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

/**
 * Constructor of InboundMessage.
 *
 * \param rxPkt
 *      First packet received for a message at the receiver. The packet type is
 *      either REQUEST or UNSCHED_DATA.
 * \param rxScheduler
 *      ReceiveScheduler that handles scheduling, priority assignment, and
 *      reception of packets for this message.
 * \param homaConfig
 *      Collection of user provided config parameters for the transport.
 */
HomaTransport::InboundMessage::InboundMessage(HomaPkt* rxPkt,
        ReceiveScheduler* rxScheduler, HomaConfigDepot* homaConfig)
    : rxScheduler(rxScheduler)
    , homaConfig(homaConfig)
    , srcAddr(rxPkt->getSrcAddr())
    , destAddr(rxPkt->getDestAddr())
    , msgIdAtSender(rxPkt->getMsgId())
    , bytesToGrant(0)
    , bytesToGrantOnWire(0)
    , bytesGrantedInFlight(0)
    , totalBytesInFlight(0)
    , bytesToReceive(0)
    , msgSize(0)
    , totalBytesOnWire(0)
    , totalUnschedBytes(0)
    , msgCreationTime(SIMTIME_ZERO)
    , reqArrivalTime(simTime())
    , lastGrantTime(simTime())
    , bytesRecvdPerPrio()
    , scheduledBytesPerPrio()
    , unschedToRecvPerPrio()
{
    switch (rxPkt->getPktType()) {
        case PktType::REQUEST:
        case PktType::UNSCHED_DATA:
            bytesToGrant = rxPkt->getUnschedFields().msgByteLen -
                rxPkt->getUnschedFields().totalUnschedBytes;
            if (bytesToGrant > 0) {
                bytesToGrantOnWire = HomaPkt::getBytesOnWire(bytesToGrant,
                    PktType::SCHED_DATA);
            }
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

/**
 * Add a received chunk of bytes to the message.
 *
 * \param byteStart
 *      Offset index in the message at which the chunk of bytes should be added.
 * \param byteEnd
 *      Index of last byte of the chunk in the message.
 * \param pktType
 *      Kind of the packet that has arrived at the receiver and carried the
 *      chunck of data bytes.
 */
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
    EV << "Received " << bytesReceived << " bytes from " << srcAddr.str()
            << " for msgId " << msgIdAtSender << " (" << bytesToReceive <<
            " bytes left to receive)" << endl;
}

/**
 * Creates a grant packet for this message and update the internal structure of
 * the message.
 *
 * \param grantSize
 *      Size of the scheduled data bytes that will specified in the grant
 *      packet.
 * \param schedPrio
 *      Priority of the scheduled packet that this grant packet is sent for.
 * \return
 *      Grant packet that is to be sent on the wire to the sender of this
 *      message.
 */
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

    int grantedBytesOnWire = HomaPkt::getBytesOnWire(grantSize,
        PktType::SCHED_DATA);
    rxScheduler->transport->outstandingGrantBytes += grantedBytesOnWire;

    // update internal structure
    this->bytesToGrant -= grantSize;
    this->bytesToGrantOnWire -= grantedBytesOnWire;
    ASSERT(bytesToGrantOnWire >= 0);
    this->bytesGrantedInFlight += grantSize;
    this->lastGrantTime = simTime();
    this->totalBytesInFlight += grantedBytesOnWire;
    return grantPkt;
}

/**
 * Create an application message once this InboundMessage is completely
 * received.
 *
 * \return
 *      Message constructed for the application and is to be passed to it.
 */
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
    }
    return rxMsg;
}

/**
 * Copy over the statistics in the InboundMessge that mirror the corresponding
 * stats in the ReceiveScheduler.
 */
void
HomaTransport::InboundMessage::updatePerPrioStats()
{
    this->bytesRecvdPerPrio = rxScheduler->bytesRecvdPerPrio;
    this->scheduledBytesPerPrio = rxScheduler->scheduledBytesPerPrio;
    this->unschedToRecvPerPrio = rxScheduler->unschedToRecvPerPrio;
    this->sumInflightUnschedPerPrio = rxScheduler->getInflightUnschedPerPrio();
    this->sumInflightSchedPerPrio = rxScheduler->getInflightSchedPerPrio();
}

/**
 * Getter method of granted but not yet received bytes (ie. outstanding
 * granted bytes) for this message.
 *
 * \return
 *      Outstanding granted bytes.
 */
uint32_t
HomaTransport::InboundMessage::schedBytesInFlight()
{
    return bytesGrantedInFlight;
}

/**
 * Getter method of bytes sent by the sender but not yet received bytes (ie.
 * outstanding bytes) for this message.
 *
 * \return
 *      Outstanding bytes.
 */
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
    this->bytesToGrantOnWire = other.bytesToGrantOnWire;
    this->bytesGrantedInFlight = other.bytesGrantedInFlight;
    this->totalBytesInFlight = other.totalBytesInFlight;
    this->bytesToReceive = other.bytesToReceive;
    this->msgSize = other.msgSize;
    this->totalBytesOnWire = other.totalBytesOnWire;
    this->totalUnschedBytes = other.totalUnschedBytes;
    this->msgCreationTime = other.msgCreationTime;
    this->reqArrivalTime = other.reqArrivalTime;
    this->lastGrantTime = other.lastGrantTime;
    this->bytesRecvdPerPrio = other.bytesRecvdPerPrio;
    this->scheduledBytesPerPrio = other.scheduledBytesPerPrio;
    this->unschedToRecvPerPrio = other.unschedToRecvPerPrio;
    this->sumInflightUnschedPerPrio = other.sumInflightUnschedPerPrio;
    this->sumInflightSchedPerPrio = other.sumInflightSchedPerPrio;
}
