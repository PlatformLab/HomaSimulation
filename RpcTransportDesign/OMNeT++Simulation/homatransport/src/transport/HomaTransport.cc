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
    int grantMinBytes = par("grantMinBytes");
    rxScheduler.setGrantMinBytes(grantMinBytes);
    linkSpeed = par("linkSpeed");
    grantTimeInterval = 8.0 * grantMinBytes / (linkSpeed * 10e-9);
    selfMsg = new cMessage("GrantTimer");
    selfMsg->setKind(SelfMsgKind::START);
    scheduleAt(simTime(), selfMsg);
}

void
HomaTransport::processStart()
{
    socket.setOutputGate(gate("udpOut"));
    socket.bind(localPort);
    selfMsg->setKind(SelfMsgKind::GRANT);
    scheduleAt(simTime() + grantTimeInterval, selfMsg);
}

void
HomaTransport::processGrantTimer()
{
    rxScheduler.sendGrant();
    scheduleAt(simTime() + grantTimeInterval, selfMsg);
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
                    sxController.processIncomingGrant(rxPkt);
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
{}


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
HomaTransport::SendController::processIncomingGrant(HomaPkt* rxPkt)
{
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
    reqPkt->setPktType(PktType::REQUEST);
    RequestFields reqFields;
    reqFields.msgByteLen = this->bytesLeft;
    reqFields.msgCreationTime = msgCreationTime;
    reqPkt->setReqFields(reqFields);
    reqPkt->setDestAddr(this->destAddr);
    reqPkt->setSrcAddr(this->srcAddr);
    reqPkt->setMsgId(this->msgId);
    sxController->transport->sendPacket(reqPkt);
}

int
HomaTransport::OutboundMessage::sendBytes(uint32_t numBytes)
{
    ASSERT(this->bytesLeft > 0);
    uint32_t bytesToSend = std::min(numBytes, this->bytesLeft); 

    // create a data pkt and send out
    HomaPkt* dataPkt = new(HomaPkt);
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
    return this->bytesLeft;
}


/**
 * HomaTransport::ReceiveScheduler
 */
HomaTransport::ReceiveScheduler::ReceiveScheduler(HomaTransport* transport)
    : transport(transport) 
    , inboundMsgQueue()
    , incompleteRxMsgs()
    , grantMinBytes(0)
{}

HomaTransport::ReceiveScheduler::~ReceiveScheduler()
{
    for (std::list<InboundMessage*>::iterator it = incompleteRxMsgs.begin()
            ; it != incompleteRxMsgs.end(); ++it) {
        InboundMessage* msgToDelete = *it;
        delete msgToDelete;
    }
}

void
HomaTransport::ReceiveScheduler::processReceivedRequest(HomaPkt* rxPkt)
{
    // if request for zero bytes, message is complete and should be sent
    // to app (no to queue it) 
    if (rxPkt->getByteLength() == 0) {
        AppMessage* rxMsg = new AppMessage();
        rxMsg->setDestAddr(rxPkt->getDestAddr());
        rxMsg->setSrcAddr(rxPkt->getSrcAddr());
        rxMsg->setByteLength(0);
        transport->send(rxMsg, "appOut");
        delete rxPkt;
        return;
    }

    // Add this message to the inboundMsgQueue and also to the list of
    // incomplete messages
    InboundMessage* inboundMsg = new InboundMessage(rxPkt, this);
    inboundMsgQueue.push(inboundMsg);                        
    incompleteRxMsgs.push_back(inboundMsg);
    delete rxPkt;
     
}

void HomaTransport::ReceiveScheduler::processReceivedData(HomaPkt* rxPkt)
{
    for (std::list<InboundMessage*>::iterator it = incompleteRxMsgs.begin();
            it != incompleteRxMsgs.end(); ++it) {
        InboundMessage* inboundMsg = *it;
        if (rxPkt->getMsgId() == inboundMsg->msgIdAtSender &&
                rxPkt->getSrcAddr() == inboundMsg->srcAddr) {
            
            incompleteRxMsgs.erase(it);
            inboundMsg->bytesToReceive -= (rxPkt->getDataFields().lastByte 
                    - rxPkt->getDataFields().firstByte + 1);

            // If bytesToReceive is zero, this message is complete and must be
            // sent to the application
            if (inboundMsg->bytesToReceive <= 0) {

                // send message to app
                AppMessage* rxMsg = new AppMessage();
                rxMsg->setDestAddr(inboundMsg->destAddr);
                rxMsg->setSrcAddr(inboundMsg->srcAddr);
                rxMsg->setByteLength(inboundMsg->msgSize);
                transport->send(rxMsg, "appOut");
                delete inboundMsg;
            } else {
                incompleteRxMsgs.push_front(inboundMsg);
            }
            return;
        }
    }
    cRuntimeError("Received unexpected data with msgId %lu from src %s",
            rxPkt->getMsgId(), rxPkt->getSrcAddr().str().c_str());
}

void
HomaTransport::ReceiveScheduler::sendGrant()
{
    if (!inboundMsgQueue.empty()) {
        InboundMessage* highPrioMsg = inboundMsgQueue.top();
        int grantSize = 
                std::min(highPrioMsg->bytesToGrant, (uint32_t)grantMinBytes);

        // prepare a grant and send out
        HomaPkt* grantPkt = new(HomaPkt);
        grantPkt->setPktType(PktType::GRANT);
        GrantFields grantFields;
        grantFields.grantBytes = this->grantMinBytes;
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
    , msgSize(rxPkt->getReqFields().msgByteLen)
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
}
