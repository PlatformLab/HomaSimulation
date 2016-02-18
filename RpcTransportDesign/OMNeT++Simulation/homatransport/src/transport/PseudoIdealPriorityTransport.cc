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
#include "PseudoIdealPriorityTransport.h"

Define_Module(PseudoIdealPriorityTransport);

/**
* Registering all of the statistics collection signals.
*/
simsignal_t PseudoIdealPriorityTransport::msgsLeftToSendSignal =
        registerSignal("msgsLeftToSend");
simsignal_t PseudoIdealPriorityTransport::bytesLeftToSendSignal =
        registerSignal("bytesLeftToSend");

PseudoIdealPriorityTransport::PseudoIdealPriorityTransport()
    : socket()
    , selfMsg(NULL)
    , localPort(-1)
    , destPort(-1)
    , msgId(0)
    , maxDataBytesInPkt(0)
{
    std::random_device rd;
    std::mt19937_64 merceneRand(rd());
    std::uniform_int_distribution<uint64_t> dist(0, UINTMAX_MAX);
    msgId = dist(merceneRand);
    HomaPkt unschePkt = HomaPkt();
    unschePkt.setPktType(PktType::UNSCHED_DATA);
    maxDataBytesInPkt =
            MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE - UDP_HEADER_SIZE -
            unschePkt.headerSize();
}

PseudoIdealPriorityTransport::~PseudoIdealPriorityTransport()
{
    cancelAndDelete(selfMsg);
    for (auto incompMsgIter = incompleteRxMsgsMap.begin();
            incompMsgIter !=  incompleteRxMsgsMap.end(); ++incompMsgIter) {
        std::list<InboundMsg*> &rxMsgList = incompMsgIter->second;
        for (auto inbndIter = rxMsgList.begin(); inbndIter != rxMsgList.end();
                ++inbndIter) {
            InboundMsg* incompleteRxMsg = *inbndIter;
            delete incompleteRxMsg;
        }
    }
}

void
PseudoIdealPriorityTransport::initialize()
{
    // Read parameters from the ned file
    localPort = par("localPort");
    destPort = par("destPort");

    // Initialize and schedule the start timer
    selfMsg = new cMessage("stopTimer");
    selfMsg->setKind(SelfMsgKind::START);
    scheduleAt(simTime(), selfMsg);
}

void
PseudoIdealPriorityTransport::processStart()
{
    socket.setOutputGate(gate("udpOut"));
    socket.bind(localPort);
}

void
PseudoIdealPriorityTransport::processStop()
{}

void
PseudoIdealPriorityTransport::finish()
{}

void
PseudoIdealPriorityTransport::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        switch(msg->getKind()) {
            case SelfMsgKind::START:
                processStart();
                break;
            case SelfMsgKind::STOP:
                processStop();
                break;
            default:
                throw cRuntimeError("Received SelfMsg of type(%d) is not valid.",
                        msg->getKind());
        }
    } else {
        if (msg->arrivedOn("appIn")) {
            processMsgFromApp(check_and_cast<AppMessage*>(msg));
        } else if (msg->arrivedOn("udpIn")) {
            processRcvdPkt(check_and_cast<HomaPkt*>(msg));
        }
    }
}

void
PseudoIdealPriorityTransport::processMsgFromApp(AppMessage* sendMsg)
{
    emit(msgsLeftToSendSignal, 1);
    emit(bytesLeftToSendSignal, sendMsg->getByteLength());
    uint32_t msgByteLen = sendMsg->getByteLength();
    simtime_t msgCreationTime = sendMsg->getMsgCreationTime();
    inet::L3Address destAddr = sendMsg->getDestAddr();
    inet::L3Address srcAddr = sendMsg->getSrcAddr();
    uint32_t firstByte = 0;
    uint32_t lastByte = 0;
    uint32_t bytesToSend = sendMsg->getByteLength();
    do {

        // Create an usncheduled pkt and fill it up with the proper parameters
        uint32_t pktDataBytes = std::min(bytesToSend, maxDataBytesInPkt);
        lastByte = firstByte + pktDataBytes - 1;
        UnschedFields unschedFields;
        unschedFields.msgByteLen = msgByteLen;
        unschedFields.msgCreationTime = msgCreationTime;
        unschedFields.totalUnschedBytes = pktDataBytes;
        unschedFields.firstByte = firstByte;
        unschedFields.lastByte = lastByte;
        bytesToSend -= pktDataBytes;
        firstByte = lastByte + 1;

        // Create a homa pkt for transmission
        HomaPkt* sxPkt = new HomaPkt();
        sxPkt->setSrcAddr(srcAddr);
        sxPkt->setDestAddr(destAddr);
        sxPkt->setMsgId(msgId);
        sxPkt->setPriority(bytesToSend);
        sxPkt->setPktType(PktType::UNSCHED_DATA);
        sxPkt->setUnschedFields(unschedFields);
        sxPkt->setByteLength(pktDataBytes + sxPkt->headerSize());

        // Send the packet out
        socket.sendTo(sxPkt, sxPkt->getDestAddr(), destPort);
    } while(bytesToSend > 0);
    delete sendMsg;
    ++msgId;
}

void
PseudoIdealPriorityTransport::processRcvdPkt(HomaPkt* rxPkt)
{
    // Find the InboundMsg corresponding to this rxPkt in the
    // incompleteRxMsgsMap.
    uint64_t msgId = rxPkt->getMsgId();
    inet::L3Address srcAddr = rxPkt->getSrcAddr();
    InboundMsg* inboundRxMsg = NULL;
    std::list<InboundMsg*> &rxMsgList = incompleteRxMsgsMap[msgId];
    for (auto inbndIter = rxMsgList.begin(); inbndIter != rxMsgList.end();
            ++inbndIter) {
        InboundMsg* incompleteRxMsg = *inbndIter;
        ASSERT(incompleteRxMsg->msgIdAtSender == msgId);
        if (incompleteRxMsg->srcAddr == srcAddr) {
            inboundRxMsg = incompleteRxMsg;
            break;
        }
    }

    // This rxPkt doesn't belong to any InboundMsg in the incompleteRxMsgsMap,
    // so we will create an InboundMsg for this rxPkt and push it to the
    // incompleteRxMsgsMap.
    if (!inboundRxMsg) {
        inboundRxMsg = new InboundMsg(rxPkt);
        rxMsgList.push_front(inboundRxMsg);
    }

    // Append the data to the inboundRxMsg and if the msg is complete, remove it
    // from the list of outstanding messages in the map, and send the complete
    // message to the application.
    if (inboundRxMsg->appendPktData(rxPkt)) {
        rxMsgList.remove(inboundRxMsg);
        if (rxMsgList.empty()) {
            incompleteRxMsgsMap.erase(msgId);
        }
        AppMessage* rxMsg = new AppMessage();
        rxMsg->setDestAddr(inboundRxMsg->destAddr);
        rxMsg->setSrcAddr(inboundRxMsg->srcAddr);
        rxMsg->setMsgCreationTime(inboundRxMsg->msgCreationTime);
        rxMsg->setTransportSchedDelay(SIMTIME_ZERO);
        rxMsg->setByteLength(inboundRxMsg->msgByteLen);
        rxMsg->setMsgBytesOnWire(inboundRxMsg->totalBytesOnWire);
        send(rxMsg, "appOut", 0);
        delete inboundRxMsg;
    }
    delete rxPkt;
}


PseudoIdealPriorityTransport::InboundMsg::InboundMsg()
    : numBytesToRecv(0)
    , msgByteLen(0)
    , totalBytesOnWire(0)
    , srcAddr()
    , destAddr()
    , msgIdAtSender(0)
    , msgCreationTime(SIMTIME_ZERO)
{}

PseudoIdealPriorityTransport::InboundMsg::InboundMsg(HomaPkt* rxPkt)
    : numBytesToRecv(0)
    , msgByteLen(0)
    , totalBytesOnWire(0)
    , srcAddr(rxPkt->getSrcAddr())
    , destAddr(rxPkt->getDestAddr())
    , msgIdAtSender(rxPkt->getMsgId())
    , msgCreationTime(SIMTIME_ZERO)
{
    ASSERT(rxPkt->getPktType() == PktType::UNSCHED_DATA);
    numBytesToRecv = rxPkt->getUnschedFields().msgByteLen;
    msgByteLen = numBytesToRecv;
    msgCreationTime = rxPkt->getUnschedFields().msgCreationTime;
}

PseudoIdealPriorityTransport::InboundMsg::~InboundMsg()
{}

bool
PseudoIdealPriorityTransport::InboundMsg::appendPktData(HomaPkt* rxPkt)
{
    UnschedFields unschedFields = rxPkt->getUnschedFields();
    ASSERT((rxPkt->getPktType() == PktType::UNSCHED_DATA) &&
            (unschedFields.msgCreationTime == msgCreationTime) &&
            (unschedFields.msgByteLen == msgByteLen));
    ASSERT((rxPkt->getSrcAddr() == srcAddr) &&
            (rxPkt->getDestAddr() == destAddr) &&
            (rxPkt->getMsgId() == msgIdAtSender));

    // Return true if rxPkt is the sole packet of a size zero message
    if (msgByteLen == 0) {
        totalBytesOnWire +=
            HomaPkt::getBytesOnWire(0, (PktType)rxPkt->getPktType());
        return true;
    }

    // append the data and return
    uint32_t dataBytesInPkt =
        unschedFields.lastByte - unschedFields.firstByte + 1;
    totalBytesOnWire +=
        HomaPkt::getBytesOnWire(dataBytesInPkt, (PktType)rxPkt->getPktType());

    numBytesToRecv -= dataBytesInPkt;
    if (numBytesToRecv < 0) {
        throw cRuntimeError("Remaining bytes to receive for an inbound msg can't be"
                " negative.");
    }

    if (numBytesToRecv == 0) {
        return true;
    } else {
        return false;
    }
}
