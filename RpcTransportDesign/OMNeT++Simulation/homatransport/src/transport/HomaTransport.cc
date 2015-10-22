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
#include "HomaTransport.h"
#include <random>
#include <cmath>

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
    , socket()
    , selfMsg(NULL)
{}

/**
 * Destructor of HomaTransport.
 */
HomaTransport::~HomaTransport()
{}

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
    sxController.initSendController((uint32_t) par("defaultReqBytes"),
            (uint32_t) par("defaultUnschedBytes"));
    HomaPkt dataPkt = HomaPkt();
    dataPkt.setPktType(PktType::SCHED_DATA);
    uint32_t maxDataBytes = MAX_ETHERNET_PAYLOAD_BYTES -
            IP_HEADER_SIZE - UDP_HEADER_SIZE - dataPkt.headerSize();
    if (grantMaxBytes > maxDataBytes) {
        grantMaxBytes = maxDataBytes;
    }

    selfMsg = new cMessage("GrantTimer");
    selfMsg->setKind(SelfMsgKind::START);
    rxScheduler.initialize(grantMaxBytes, nicLinkSpeed, selfMsg);
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
            sxController.processSendMsgFromApp(
                    check_and_cast<AppMessage*>(msg));

        } else if (msg->arrivedOn("udpIn")) {
            HomaPkt* rxPkt = check_and_cast<HomaPkt*>(msg);
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

/**
 * NOTE: calling this function with numDataBytes=0 assumes that you are sending
 * a packet with no data on the wire and you want to see how many bytes an empty
 * packet is on the wire.
 */
uint32_t
HomaTransport::getBytesOnWire(uint32_t numDataBytes, PktType homaPktType)
{

    HomaPkt homaPkt = HomaPkt();
    homaPkt.setPktType(homaPktType);
    uint32_t homaPktHdrSize = homaPkt.headerSize();

    uint32_t bytesOnWire = 0;

    uint32_t maxDataInHomaPkt = MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE
            - UDP_HEADER_SIZE - homaPktHdrSize; 
    
    uint32_t numFullPkts = numDataBytes / maxDataInHomaPkt;
    bytesOnWire += numFullPkts * (MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_HDR_SIZE
            + ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP);

    uint32_t numPartialBytes = numDataBytes - numFullPkts * maxDataInHomaPkt; 

    // if all numDataBytes fit in full pkts, we should return at this point
    if (numFullPkts > 0 && numPartialBytes == 0) {
        return bytesOnWire;
    }

    numPartialBytes += homaPktHdrSize + IP_HEADER_SIZE +
            UDP_HEADER_SIZE;

    if (numPartialBytes < MIN_ETHERNET_PAYLOAD_BYTES) {
        numPartialBytes = MIN_ETHERNET_PAYLOAD_BYTES;
    }
    
    uint32_t numPartialBytesOnWire = numPartialBytes + ETHERNET_HDR_SIZE +
            ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP;
    
    bytesOnWire += numPartialBytesOnWire;

    return bytesOnWire;
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
{}

void
HomaTransport::SendController::initSendController(uint32_t defaultReqBytes,
        uint32_t defaultUnschedBytes)
{
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
    int bytesLeftNew = 
        outboundMsg->sendSchedBytes(rxPkt->getGrantFields().grantBytes);
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
    , bytesLeft(outMsg->getByteLength())
    , nextByteToSend(0) 
    , dataBytesInReq(dataBytesInReq)
    , unschedDataBytes(unschedDataBytes)
    , destAddr(outMsg->getDestAddr())
    , srcAddr(outMsg->getSrcAddr())
    , msgCreationTime(outMsg->getCreationTime())
{}

HomaTransport::OutboundMessage::OutboundMessage()
    : sxController(NULL)
    , msgId(0)
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
    HomaPkt* reqPkt = new(HomaPkt);
    reqPkt->setPktType(PktType::REQUEST);

    // create request fields and append the data
    RequestFields reqFields;
    reqFields.numReqBytes = this->dataBytesInReq;
    reqFields.totalUnschedBytes = this->dataBytesInReq + this->unschedDataBytes;
    reqFields.msgByteLen = this->bytesLeft;
    reqFields.msgCreationTime = msgCreationTime;
    reqPkt->setReqFields(reqFields);

    // set homa fields
    reqPkt->setDestAddr(this->destAddr);
    reqPkt->setSrcAddr(this->srcAddr);
    reqPkt->setMsgId(this->msgId);
    reqPkt->setPriority(0);

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
        HomaPkt* unschedPkt = new(HomaPkt);
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
        unschedFields.totalUnschedBytes = 
                this->dataBytesInReq + this->unschedDataBytes;
        unschedFields.firstByte = this->nextByteToSend;
        unschedFields.lastByte = this->nextByteToSend + bytesToSend - 1;
        unschedPkt->setUnschedDataFields(unschedFields);

        // set homa fields
        unschedPkt->setDestAddr(this->destAddr);
        unschedPkt->setSrcAddr(this->srcAddr);
        unschedPkt->setMsgId(this->msgId);
        unschedPkt->setPriority(1);

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
HomaTransport::OutboundMessage::sendSchedBytes(uint32_t numBytes)
{
    ASSERT(this->bytesLeft > 0);
    uint32_t bytesToSend = std::min(numBytes, this->bytesLeft); 

    // create a data pkt and send out
    HomaPkt* dataPkt = new(HomaPkt);
    dataPkt->setPktType(PktType::SCHED_DATA);
    dataPkt->setSrcAddr(this->srcAddr);
    dataPkt->setDestAddr(this->destAddr);
    dataPkt->setMsgId(this->msgId);
    dataPkt->setPriority(2);
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
    , inboundMsgQueue()
    , incompleteRxMsgs()
    , grantMaxBytes(0)
{
    trafficPacer = (TrafficPacer*) ::operator new (sizeof(TrafficPacer));
}

void
HomaTransport::ReceiveScheduler::initialize(uint32_t grantMaxBytes,
        uint32_t nicLinkSpeed, cMessage* grantTimer)
{
    this->grantMaxBytes = grantMaxBytes;
    this->trafficPacer = new(this->trafficPacer) TrafficPacer(nicLinkSpeed, 
            transport->maxOutstandingRecvBytes);
    this->grantTimer = grantTimer;
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
}

void
HomaTransport::ReceiveScheduler::processReceivedRequest(HomaPkt* rxPkt)
{
    EV_INFO << "Received request with msgId " << rxPkt->getMsgId() << " from "
            << rxPkt->getSrcAddr().str() << " to " << rxPkt->getDestAddr().str()
            << " for " << rxPkt->getReqFields().msgByteLen << " bytes." << endl;

    uint32_t numBytesInReqPkt = rxPkt->getReqFields().numReqBytes;

    // Check if this message already added to incompleteRxMsg. if not create new
    // inbound message in both the msgQueue and incomplete messages.
    InboundMessage* inboundMsg = lookupIncompleteRxMsg(rxPkt);

    if (!inboundMsg) {
        inboundMsg = new InboundMessage(rxPkt, this);
        if (inboundMsg->bytesToGrant > 0) {
            inboundMsgQueue.push(inboundMsg);                        
        }
        incompleteRxMsgs[rxPkt->getMsgId()].push_front(inboundMsg);


        uint32_t unschedBytesToArrive = rxPkt->getReqFields().totalUnschedBytes -
                numBytesInReqPkt;
        // for any new request, the trailing unsched bytes (IF ANY) that are in
        // the flight to the receiver must be accounted in the future grants we
        // will send as they might interfer with sched data.
        if (unschedBytesToArrive > 0) {
            uint32_t unschedBytesOnWire = transport->getBytesOnWire(
                    unschedBytesToArrive, PktType::UNSCHED_DATA); 
            trafficPacer->unschedPendingBytes(unschedBytesOnWire);
            transport->emit(totalOutstandingBytesSignal,
                    trafficPacer->getOutstandingBytes());
        }
    } else {

        // this request bytes are previously accounted for in the trafficPacer
        // and now we need to return them to the pacer
        transport->emit(totalOutstandingBytesSignal, 
                trafficPacer->getOutstandingBytes());
        trafficPacer->bytesArrived(transport->getBytesOnWire(numBytesInReqPkt,
                PktType::REQUEST));

    }

    // If this req. packet also contains data, we need to add the data to the
    // inbound message and update the incompleteRxMsgs. 
    if (numBytesInReqPkt > 0) {
        inboundMsg->fillinRxBytes(0, numBytesInReqPkt - 1);
    }

    if (inboundMsg->bytesToReceive <= 0) {
        ASSERT(inboundMsg->bytesToGrant == 0);

        // this message is complete, so send it to the application
        AppMessage* rxMsg = new AppMessage();
        rxMsg->setDestAddr(rxPkt->getDestAddr());
        rxMsg->setSrcAddr(rxPkt->getSrcAddr());
        rxMsg->setMsgCreationTime(rxPkt->getReqFields().msgCreationTime);
        rxMsg->setByteLength(inboundMsg->msgSize);
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
        uint32_t unschedBytesOnWire = 0;
        uint32_t reqPktDataBytes = rxPkt->getUnschedDataFields().numReqBytes;
        unschedBytesOnWire += transport->getBytesOnWire(reqPktDataBytes,
                PktType::REQUEST);
        uint32_t unschedBytesToArrive = 
                rxPkt->getUnschedDataFields().totalUnschedBytes -
                reqPktDataBytes - pktUnschedBytes;
        if (unschedBytesToArrive > 0) {
            unschedBytesOnWire += transport->getBytesOnWire(
                    unschedBytesToArrive, PktType::UNSCHED_DATA);
        }

        trafficPacer->unschedPendingBytes(unschedBytesOnWire);
        transport->emit(totalOutstandingBytesSignal,
                trafficPacer->getOutstandingBytes());
    } else {
        // this is a packet that we expected to receive and have previously
        // accounted for in the trafficPacer. Now we need to return it to the
        // pacer.

        transport->emit(totalOutstandingBytesSignal, 
                trafficPacer->getOutstandingBytes());
        trafficPacer->bytesArrived(transport->getBytesOnWire(
                pktUnschedBytes, PktType::UNSCHED_DATA));
    }



    // Append the unsched data to the inboundMsg
    inboundMsg->fillinRxBytes(rxPkt->getUnschedDataFields().firstByte,
            rxPkt->getUnschedDataFields().lastByte);

    if (inboundMsg->bytesToReceive <= 0) {
        ASSERT(inboundMsg->bytesToGrant == 0);

        // this message is complete, so send it to the application
        AppMessage* rxMsg = new AppMessage();
        rxMsg->setDestAddr(rxPkt->getDestAddr());
        rxMsg->setSrcAddr(rxPkt->getSrcAddr());
        rxMsg->setMsgCreationTime(rxPkt->getUnschedDataFields().msgCreationTime);
        rxMsg->setByteLength(inboundMsg->msgSize);
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
    ASSERT(bytesReceived > 0);

    // this is a packet we have already committed in the trafficPacer by sending
    // agrant for it. Now the grant for this packet must be returned to the
    // pacer.
    uint32_t totalOnwireBytesReceived =
            transport->getBytesOnWire(bytesReceived, PktType::SCHED_DATA);
    transport->outstandingGrantBytes -= totalOnwireBytesReceived;
    transport->emit(totalOutstandingBytesSignal, trafficPacer->getOutstandingBytes());
    trafficPacer->bytesArrived(totalOnwireBytesReceived);

    EV_INFO << "Received " << bytesReceived << " bytes from "
            << inboundMsg->srcAddr.str() << " for msgId "
            << rxPkt->getMsgId() << " (" << inboundMsg->bytesToReceive
            << " bytes left to receive)" << endl;

    inboundMsg->fillinRxBytes(rxPkt->getSchedDataFields().firstByte, 
            rxPkt->getSchedDataFields().lastByte);

    // If bytesToReceive is zero, this message is complete and must be
    // sent to the application. We should also delete the message from the
    // containers
    if (inboundMsg->bytesToReceive <= 0) {

        // send message to app
        AppMessage* rxMsg = new AppMessage();
        rxMsg->setDestAddr(inboundMsg->destAddr);
        rxMsg->setSrcAddr(inboundMsg->srcAddr);
        rxMsg->setMsgCreationTime(inboundMsg->msgCreationTime);
        rxMsg->setByteLength(inboundMsg->msgSize);
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
    uint32_t grantSize;
    simtime_t currentTime = simTime();

    if (inboundMsgQueue.empty()) {
        return;
    }

    InboundMessage* highPrioMsg = inboundMsgQueue.top();
    grantSize = 
            std::min(highPrioMsg->bytesToGrant, grantMaxBytes);

    // The size of scheduled data bytes on wire.
    uint32_t schedBytesOneWire = transport->getBytesOnWire(
            grantSize, PktType::SCHED_DATA);
    if (trafficPacer->okToGrant(currentTime, schedBytesOneWire)) {

        // prepare a grant and send out
        HomaPkt* grantPkt = new(HomaPkt);
        grantPkt->setPktType(PktType::GRANT);
        GrantFields grantFields;
        grantFields.grantBytes = grantSize;
        grantPkt->setGrantFields(grantFields);
        grantPkt->setDestAddr(highPrioMsg->srcAddr);
        grantPkt->setSrcAddr(highPrioMsg->destAddr);
        grantPkt->setMsgId(highPrioMsg->msgIdAtSender);
        grantPkt->setByteLength(grantPkt->headerSize());
        grantPkt->setPriority(0);
        transport->sendPacket(grantPkt);

        transport->outstandingGrantBytes += schedBytesOneWire;
        transport->emit(outstandingGrantBytesSignal, transport->outstandingGrantBytes);

        // set the grantTimer for the next grant
        transport->scheduleAt(trafficPacer->grantSent(
               schedBytesOneWire, currentTime), grantTimer);

        // update highPrioMsg and msgQueue
        highPrioMsg->bytesToGrant -= grantSize;
        if (highPrioMsg->bytesToGrant <= 0) {
            inboundMsgQueue.pop();
        }

        EV_INFO << " Sent a grant, among " << inboundMsgQueue.size()
                << " possible choices, for msgId " << highPrioMsg->msgIdAtSender
                << " at host " << highPrioMsg->srcAddr.str() << " for "
                << grantSize << " Bytes." << "(" << highPrioMsg->bytesToGrant
                << " Bytes left to grant.)" << endl;
    }
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
    , bytesToReceive(0)
    , msgSize(0)
    , bytesInReq(0)  
    , totalUnschedBytes(0)
    , msgCreationTime(SIMTIME_ZERO)
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
    , bytesToReceive(0)
    , msgSize(0)
    , bytesInReq(0)  
    , totalUnschedBytes(0)
    , msgCreationTime(SIMTIME_ZERO)
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
