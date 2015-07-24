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

Define_Module(HomaTransport);

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

                case PktType::GRANT:
                    sxController.processReceivedGrant(rxPkt);
                    break;

                case PktType::DATA:
                    rxScheduler.processReceivedData(rxPkt);
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
    , msgId(0)
    , outboundMsgMap()
{}

HomaTransport::SendController::~SendController()
{}

void
HomaTransport::SendController::processSendMsgFromApp(AppMessage* sendMsg)
{
//    outboundMsgMap.emplace(
//            std::make_pair(msgId, 
//            HomaTransport::OutboundMessage(sendMsg, this, msgId)));
//
    outboundMsgMap.emplace(std::piecewise_construct, 
            std::forward_as_tuple(msgId), 
            std::forward_as_tuple(sendMsg, this, msgId));
    outboundMsgMap.at(msgId).sendRequest(sendMsg->getCreationTime()); 
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

    int outboundMsgByteLeft = 
        outboundMsg->sendBytes(rxPkt->getGrantFields().grantBytes);

    if (outboundMsgByteLeft == 0) {
        outboundMsgMap.erase(rxPkt->getMsgId());
    }
    delete rxPkt;   
}


/**
 * HomaTransport::OutboundMessage
 */
HomaTransport::OutboundMessage::OutboundMessage(AppMessage* outMsg,
        SendController* sxController, uint64_t msgId)
    : sxController(sxController)
    , bytesLeft(outMsg->getByteLength())
    , nextByteToSend(0) 
    , destAddr(outMsg->getDestAddr())
    , srcAddr(outMsg->getSrcAddr())
    , msgId(msgId)
{}

HomaTransport::OutboundMessage::OutboundMessage()
    : sxController(NULL)
    , bytesLeft(0) 
    , nextByteToSend(0)
    , destAddr()
    , srcAddr()
    , msgId(0)
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
HomaTransport::OutboundMessage::sendRequest(simtime_t msgCreationTime)
{
    HomaPkt* reqPkt = new(HomaPkt);
    reqPkt->setByteLength(0);
    reqPkt->setPktType(PktType::REQUEST);
    RequestFields reqFields;
    reqFields.msgByteLen = this->bytesLeft;
    reqFields.msgCreationTime = msgCreationTime;
    reqPkt->setReqFields(reqFields);
    reqPkt->setDestAddr(this->destAddr);
    reqPkt->setSrcAddr(this->srcAddr);
    reqPkt->setMsgId(this->msgId);
    sxController->transport->sendPacket(reqPkt);
    EV_INFO << "Send request with msgId " << this->msgId << " from "
            << this->srcAddr.str() << " to " << this->destAddr.str() << endl;
}

int
HomaTransport::OutboundMessage::sendBytes(uint32_t numBytes)
{
    ASSERT(this->bytesLeft > 0);
    uint32_t bytesToSend = std::min(numBytes, this->bytesLeft); 

    // create a data pkt and send out
    HomaPkt* dataPkt = new(HomaPkt);
    dataPkt->setByteLength(bytesToSend);
    dataPkt->setSrcAddr(this->srcAddr);
    dataPkt->setDestAddr(this->destAddr);
    dataPkt->setMsgId(this->msgId);
    dataPkt->setPktType(PktType::DATA);
    DataFields dataFields;
    dataFields.firstByte = this->nextByteToSend;
    dataFields.lastByte = dataFields.firstByte + bytesToSend - 1;
    dataPkt->setDataFields(dataFields);
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
    for (std::list<InboundMessage*>::iterator it = incompleteRxMsgs.begin()
            ; it != incompleteRxMsgs.end(); ++it) {
        InboundMessage* msgToDelete = *it;
        delete msgToDelete;
    }

    delete byteBucket;
}

void
HomaTransport::ReceiveScheduler::processReceivedRequest(HomaPkt* rxPkt)
{
    // if request for zero bytes, message is complete and should be sent
    // to app (no to queue it) 
    EV_INFO << "Received request with msgId " << rxPkt->getMsgId() << " from "
            << rxPkt->getSrcAddr().str() << " to " << rxPkt->getDestAddr().str()
            << " for " << rxPkt->getReqFields().msgByteLen << " bytes." << endl;
    if (rxPkt->getReqFields().msgByteLen == 0) {
        AppMessage* rxMsg = new AppMessage();
        rxMsg->setDestAddr(rxPkt->getDestAddr());
        rxMsg->setSrcAddr(rxPkt->getSrcAddr());
        rxMsg->setMsgCreationTime(rxPkt->getReqFields().msgCreationTime);
        rxMsg->setByteLength(0);
        transport->send(rxMsg, "appOut", 0);
        delete rxPkt;
        return;
    }

    // Add this message to the inboundMsgQueue and also to the list of
    // incomplete messages
    InboundMessage* inboundMsg = new InboundMessage(rxPkt, this);
    inboundMsgQueue.push(inboundMsg);                        
    incompleteRxMsgs.push_back(inboundMsg);
    delete rxPkt;
    sendAndScheduleGrant();
     
}

void HomaTransport::ReceiveScheduler::processReceivedData(HomaPkt* rxPkt)
{
    for (std::list<InboundMessage*>::iterator it = incompleteRxMsgs.begin();
            it != incompleteRxMsgs.end(); ++it) {
        InboundMessage* inboundMsg = *it;
        if (rxPkt->getMsgId() == inboundMsg->msgIdAtSender &&
                rxPkt->getSrcAddr() == inboundMsg->srcAddr) {
            
            incompleteRxMsgs.erase(it);
            uint32_t bytesReceived = (rxPkt->getDataFields().lastByte 
                    - rxPkt->getDataFields().firstByte + 1);
            inboundMsg->bytesToReceive -= bytesReceived;
            EV_INFO << "Received " << bytesReceived << " bytes from "
                    << inboundMsg->srcAddr.str() << " for msgId " 
                    << rxPkt->getMsgId() << " (" << inboundMsg->bytesToReceive
                    << " bytes left to receive)" << endl;

            // If bytesToReceive is zero, this message is complete and must be
            // sent to the application
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
            } else {
                incompleteRxMsgs.push_front(inboundMsg);
            }
            delete rxPkt;
            return;
        }
    }
    cRuntimeError("Received unexpected data with msgId %lu from src %s",
            rxPkt->getMsgId(), rxPkt->getSrcAddr().str().c_str());
}

void
HomaTransport::ReceiveScheduler::sendAndScheduleGrant()
{ 
    ASSERT(grantTimer->getKind() == HomaTransport::SelfMsgKind::GRANT);
    uint32_t grantSize;
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


        byteBucket->getGrant(grantSize, currentTime);

        // prepare a grant and send out
        HomaPkt* grantPkt = new(HomaPkt);
        grantPkt->setByteLength(0);
        grantPkt->setPktType(PktType::GRANT);
        GrantFields grantFields;
        grantFields.grantBytes = grantSize;
        grantPkt->setGrantFields(grantFields);
        grantPkt->setDestAddr(highPrioMsg->srcAddr);
        grantPkt->setSrcAddr(highPrioMsg->destAddr);
        grantPkt->setMsgId(highPrioMsg->msgIdAtSender);
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
    , bytesToGrant(rxPkt->getReqFields().msgByteLen)
    , bytesToReceive(rxPkt->getReqFields().msgByteLen)
    , msgSize(rxPkt->getReqFields().msgByteLen)
    , msgCreationTime(rxPkt->getReqFields().msgCreationTime)
{
    ASSERT(rxPkt->getPktType() == PktType::REQUEST);
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
    this->msgCreationTime = other.msgCreationTime;
}
