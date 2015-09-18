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

simsignal_t HomaTransport::msgsLeftToSendSignal = 
        registerSignal("msgsLeftToSend");
simsignal_t HomaTransport::bytesLeftToSendSignal = 
        registerSignal("bytesLeftToSend");

HomaTransport::HomaTransport()
    : sxController(this)
    , rxScheduler(this)
    , socket()
    , selfMsg(NULL)
{}

HomaTransport::~HomaTransport()
{}

void
HomaTransport::initialize()
{
    localPort = par("localPort");
    destPort = par("destPort");
    nicLinkSpeed = par("nicLinkSpeed");
    uint32_t grantMaxBytes = (uint32_t) par("grantMaxBytes");
    uint32_t maxDataBytes = MAX_ETHERNET_PAYLOAD_BYTES - 
            IP_HEADER_SIZE - UDP_HEADER_SIZE;
    if (grantMaxBytes > maxDataBytes) {
        grantMaxBytes = maxDataBytes;
    }

    //maxGrantTimeInterval = 8.0 * grantMaxBytes * 10e-9 / nicLinkSpeed;
    selfMsg = new cMessage("GrantTimer");
    selfMsg->setKind(SelfMsgKind::START);
    rxScheduler.initialize(grantMaxBytes, nicLinkSpeed, selfMsg);
    scheduleAt(simTime(), selfMsg);
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
{
    std::random_device rd;
    std::mt19937_64 merceneRand(rd());
    std::uniform_int_distribution<uint64_t> dist(0, UINTMAX_MAX);
    msgId = dist(merceneRand);

}

HomaTransport::SendController::~SendController()
{}

void
HomaTransport::SendController::processSendMsgFromApp(AppMessage* sendMsg)
{

    transport->emit(msgsLeftToSendSignal, outboundMsgMap.size());
    transport->emit(bytesLeftToSendSignal, bytesLeftToSend);
    outboundMsgMap.emplace(std::piecewise_construct, 
            std::forward_as_tuple(msgId), 
            std::forward_as_tuple(sendMsg, this, msgId));
    outboundMsgMap.at(msgId).sendRequestAndUnsched(); 
    bytesLeftToSend += outboundMsgMap.at(msgId).bytesLeft;
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
        outboundMsg->sendBytes(rxPkt->getGrantFields().grantBytes);
    int bytesSent = bytesLeftOld - bytesLeftNew;
    bytesLeftToSend -= bytesSent;
    ASSERT(bytesSent > 0 && bytesLeftToSend >= 0);

    if (bytesLeftNew <= 0) {
        outboundMsgMap.erase(rxPkt->getMsgId());
    }
    delete rxPkt;   
}

uint32_t
HomaTransport::SendController::getReqDataBytes(AppMessage* sxMsg)
{
    return 10;
}

uint32_t
HomaTransport::SendController::getUnschedBytes(AppMessage* sxMsg)
{
    return 100;
}



/**
 * HomaTransport::OutboundMessage
 */
HomaTransport::OutboundMessage::OutboundMessage(AppMessage* outMsg,
        SendController* sxController, uint64_t msgId)
    : sxController(sxController)
    , msgId(msgId)
    , bytesLeft(outMsg->getByteLength())
    , nextByteToSend(0) 
    , dataBytesInReq(sxController->getReqDataBytes(outMsg))
    , unschedDataBytes(sxController->getUnschedBytes(outMsg))
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
    this->bytesLeft = other.bytesLeft;
    this->nextByteToSend = other.nextByteToSend;
    this->destAddr = other.destAddr;
    this->srcAddr = other.srcAddr;
    this->msgId = other.msgId;
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
        unschedFields.totalUnschedBytes = this->dataBytesInReq + this->unschedDataBytes;
        unschedFields.firstByte = this->nextByteToSend;
        unschedFields.lastByte = this->nextByteToSend + bytesToSend - 1;
        unschedPkt->setUnschedDataFields(unschedFields);

        // set homa fields
        unschedPkt->setDestAddr(this->destAddr);
        unschedPkt->setSrcAddr(this->srcAddr);
        unschedPkt->setMsgId(this->msgId);

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
HomaTransport::OutboundMessage::sendBytes(uint32_t numBytes)
{
    ASSERT(this->bytesLeft > 0);
    uint32_t bytesToSend = std::min(numBytes, this->bytesLeft); 

    // create a data pkt and send out
    HomaPkt* dataPkt = new(HomaPkt);
    dataPkt->setPktType(PktType::SCHED_DATA);
    dataPkt->setSrcAddr(this->srcAddr);
    dataPkt->setDestAddr(this->destAddr);
    dataPkt->setMsgId(this->msgId);
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
    , byteBucket(NULL)
    , inboundMsgQueue()
    , incompleteRxMsgs()
    , grantMaxBytes(0)
{
    byteBucket = (ByteBucket*) ::operator new (sizeof(ByteBucket));
}

void
HomaTransport::ReceiveScheduler::initialize(uint32_t grantMaxBytes,
        uint32_t nicLinkSpeed, cMessage* grantTimer)
{
    this->grantMaxBytes = grantMaxBytes;
    this->byteBucket = new(this->byteBucket) ByteBucket(nicLinkSpeed);
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
    delete byteBucket;
}

void
HomaTransport::ReceiveScheduler::processReceivedRequest(HomaPkt* rxPkt)
{
    EV_INFO << "Received request with msgId " << rxPkt->getMsgId() << " from "
            << rxPkt->getSrcAddr().str() << " to " << rxPkt->getDestAddr().str()
            << " for " << rxPkt->getReqFields().msgByteLen << " bytes." << endl;

    // Check if this message already added to incompleteRxMsg. if not create new
    // inbound message in both the msgQueue and incomplete messages.
    InboundMessage* inboundMsg = lookupIncompleteRxMsg(rxPkt);

    if (!inboundMsg) {
        inboundMsg = new InboundMessage(rxPkt, this);
        if (inboundMsg->bytesToGrant > 0) {
            inboundMsgQueue.push(inboundMsg);                        
        }
        incompleteRxMsgs[rxPkt->getMsgId()].push_front(inboundMsg);
    }

    // If this req. packet also contains data, we need to add the data to the
    // inbound message and update the incompleteRxMsgs. 
    uint32_t numBytesInReqPkt = rxPkt->getReqFields().numReqBytes;
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
    }

    // Append the unsched data to the inboundMsg
    ASSERT(rxPkt->getUnschedDataFields().lastByte -
            rxPkt->getUnschedDataFields().firstByte + 1);
    inboundMsg->fillinRxBytes(rxPkt->getUnschedDataFields().lastByte,
            rxPkt->getUnschedDataFields().firstByte);

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
}

void
HomaTransport::ReceiveScheduler::sendAndScheduleGrant()
{ 
    ASSERT(grantTimer->getKind() == HomaTransport::SelfMsgKind::GRANT);
    uint32_t grantSize;
    uint32_t grantSizeOnWire; //This include data and all other overheads
    simtime_t currentTime = simTime();
    while (true) {
        if (inboundMsgQueue.empty()) {
            return;
        }

        if (byteBucket->getGrantTime() > currentTime) {
            transport->cancelEvent(grantTimer);    
            transport->scheduleAt(byteBucket->getGrantTime(), grantTimer);
            return;
        }

        InboundMessage* highPrioMsg = inboundMsgQueue.top();
        grantSize = 
                std::min(highPrioMsg->bytesToGrant, grantMaxBytes);
//        if (inboundMsgQueue.size() > 1 ) {
//            InboundMessage* msg;
//            for (std::list<InboundMessage*>::iterator it = incompleteRxMsgs.begin()
//                    ; it != incompleteRxMsgs.end(); ++it) {
//                msg = *it;
//            }
//
//        }

        // Find what the real number of bytes that will go on the wire for a
        // ethernet frame that has grantSize number of data bytes in it. The
        // ByteBucket must include this overhead bytes in it's grant mechanism.
        if (grantSize < 
                MIN_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE - UDP_HEADER_SIZE) {

            grantSizeOnWire = ETHERNET_PREAMBLE_SIZE + ETHERNET_HDR_SIZE +
                    MIN_ETHERNET_PAYLOAD_BYTES + ETHERNET_CRC_SIZE + INTER_PKT_GAP;
        } else {
            grantSizeOnWire = ETHERNET_PREAMBLE_SIZE + ETHERNET_HDR_SIZE +
                    grantSize + IP_HEADER_SIZE + UDP_HEADER_SIZE +
                    ETHERNET_CRC_SIZE + INTER_PKT_GAP;
        }
        byteBucket->getGrant(grantSizeOnWire, currentTime);

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
        transport->sendPacket(grantPkt);

        // update highPrioMsg
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
