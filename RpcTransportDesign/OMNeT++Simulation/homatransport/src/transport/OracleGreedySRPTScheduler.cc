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

#include "OracleGreedySRPTScheduler.h"
#include "transport/MinimalTransport.h"

Define_Module(OracleGreedySRPTScheduler);

OracleGreedySRPTScheduler::OracleGreedySRPTScheduler()
    : globalMsgId(0) 
    , txMsgSet()
    , senderMsgMap()
    , receiverMsgMap()
    , msgIdMap()
    , hostAddrTransports()
    , scheduledMsgs()

{}

OracleGreedySRPTScheduler::~OracleGreedySRPTScheduler()
{}

void
OracleGreedySRPTScheduler::initialize()
{
    return;
}

void
OracleGreedySRPTScheduler::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage() || !msg->arrivedOn("appIn"))
        throw cRuntimeError("Unexpected message has arrived");
    delete msg;
}

void
OracleGreedySRPTScheduler::finish()
{
    return;
}

/**
 * returns next available motonically increasing global message id.
 *
 * \return
 *      next available global id for message.
 */
uint64_t
OracleGreedySRPTScheduler::getNewMsgId()
{
    uint64_t currentId = globalMsgId;
    globalMsgId++;
    return currentId;
}

/**
 * For an ip4 source address, returns the end host transport for that address.
 *
 * \return
 *      pointer to end host transport
 * \except
 *      Runtime error if address type is not IPv4.
 */
MinimalTransport*
OracleGreedySRPTScheduler::addrToTransport(inet::L3Address ipAddr)
{
    if (ipAddr.getType() != inet::L3Address::AddressType::IPv4) {
        throw cRuntimeError("Input arg must have IPv4 type");
    }
    return hostAddrTransports.at(ipAddr.toIPv4().getInt()); 
}

/**
 * Adds an end host transport to the set of all transports that this Centralized
 * Oracle scheduler manages. This method must be called in the initialization
 * phase of the end host transport.
 *
 * \param transport
 *      The end transport that is to be added to the scheduling list of this
 *      oracle transport. 
 * \return
 *      False if transport has been previously subscribed and therefor can't be
 *      subscribed again.
 */
bool
OracleGreedySRPTScheduler::subscribeEndhostTransport(
    MinimalTransport* transport, inet::L3Address transportAddr)
{
    uint32_t ipv4Addr = transportAddr.toIPv4().getInt();
    try {
        hostAddrTransports.at(ipv4Addr);
    } catch (std::exception& e) {
        hostAddrTransports[ipv4Addr] = transport;
        return true;
    }
    return false;
}

/**
 * This function must be called whenever a new message has entered the
 * simulation from an application at the end host. This method informs the
 * oracle scheduler about the new created message. At every new message arrival,
 * the oracle recomputes a new schedule for next packet transmission.
 *
 * \param appMesg
 *      pointer to the message created in application that needs to be
 *      transmitted over the network.
 */
void
OracleGreedySRPTScheduler::scheduleNewMesg(Rpc* appMesg)
{
    InflightMessage* newMesg = new InflightMessage(appMesg, this);
    txMsgSet.insert(newMesg);
    senderMsgMap[newMesg->sxTransport].insert(newMesg);
    receiverMsgMap[newMesg->rxTransport].insert(newMesg);
    msgIdMap[newMesg->msgId] = newMesg;
    recomputeGlobalSrptSchedule();
}

/**
 * For an endhost sender transport, this method returns a chunk of the scheduled
 * message to be transmitted by that sender transport.
 *
 * \param sxTransport
 *      The end host transport that requesting for a scheduled chunk to be
 *      transmitted.
 * \param chunkSize
 *      Size of the chunk that sxTransport is requesting for transmit.
 * \return
 *      Pointer to chunk of the scheduled message to be transmitted by
 *      sxTransport. Null means no message scheduled for sxTransport. When not
 *      null, caller is responsible for allocated memory for the returned chunk.
 */
OracleGreedySRPTScheduler::MesgChunk*
OracleGreedySRPTScheduler::getNextChunkToSend(MinimalTransport* sxTransport,
    uint16_t chunkSize)
{
    InflightMessage* msg = NULL;
    try {
        msg = scheduledMsgs.at(sxTransport);
    } catch(std::exception& e) {
        return NULL;
    }

    // Side effect: Calling this function reduces the remaining bytes of the
    // messages and therefore might change the position of this message in
    // txMsgSet, senderMsgMap, and receiverMsgMap. These changes must be
    // appropriately applied in senderMsgMap and receiverMsgMap.
    txMsgSet.erase(msg);
    auto& allSxMsgs = senderMsgMap.at(msg->sxTransport);
    allSxMsgs.erase(msg);
    auto& allRxMsg = receiverMsgMap.at(msg->rxTransport);
    allRxMsg.erase(msg);
    
    uint32_t byteOffset = msg->getNextChunkToSend(chunkSize);
    MesgChunk* chunk = new MesgChunk();
    chunk->msgId = msg->msgId; 
    chunk->offsetByte = byteOffset;
    chunk->chunkLen = chunkSize; 
    chunk->srcAddr = msg->srcAddr;
    chunk->destAddr = msg->destAddr;

    // If msg has been completely transmitted at the sender side, the
    // oracle transport can go ahead and schedule possible next message on the
    // sender that has finished transmission.
    if (msg->bytesToSend <= 0) {
        chunk->lastChunk = true;
        if (allSxMsgs.empty()) {
            // If the sender transport of this message has no more outstanding
            // mesgs, then clean up the this transport from the senderMsgMap. 
            senderMsgMap.erase(msg->sxTransport);
        }
        recomputeGlobalSrptSchedule();
    } else {
        allSxMsgs.insert(msg);
        txMsgSet.insert(msg);
    }
    allRxMsg.insert(msg);

    return chunk;
}

/**
 * At the receiver side of the message, appends newly received data chunk to the
 * message.
 *
 * \param msgChunk
 *      pointer to the received data chunk.
 * \return
 *      If the received chunk was the last piece of the message and the
 *      message is complete after this message, this method constructs an
 *      Rpc corresponding to this message and returns the pointer to the
 *      Rpc. Otherwise, it returns null. It is the responsibility of the caller
 *      to free up the allocated memory for the returned Rpc.
 */
Rpc*
OracleGreedySRPTScheduler::appendRecvdChunk(
    OracleGreedySRPTScheduler::MesgChunk* msgChunk)
{
    Rpc* rxMsg = NULL;
    InflightMessage* msg = msgIdMap.at(msgChunk->msgId);
    if (msg->appendRecvdChunk(msgChunk->offsetByte, msgChunk->chunkLen)) {
        msgChunk->lastChunk = true;
        ASSERT(txMsgSet.find(msg) == txMsgSet.end());
        msgIdMap.erase(msgChunk->msgId);
        auto& allRxMsg = receiverMsgMap.at(msg->rxTransport);
        allRxMsg.erase(msg);
        if (allRxMsg.empty()) {
            // If this receiver expects no more outstanding message, the clean
            // up this transport from the receiverMsgMap
            receiverMsgMap.erase(msg->rxTransport);
        }
        rxMsg = new Rpc();
        rxMsg->setDestAddr(msg->destAddr);
        rxMsg->setSrcAddr(msg->srcAddr);
        rxMsg->setMsgCreationTime(msg->msgCreationTime);
        rxMsg->setTransportSchedDelay(SIMTIME_ZERO);
        rxMsg->setByteLength(msg->msgByteLen);
        rxMsg->setMsgBytesOnWire(msg->msgByteLen);
        delete msg;
    }
    return rxMsg;
}


/**
 * Finds a message transmission schedule (ie .a match) from senders to receivers
 * based on SRPT policy.
 */
void
OracleGreedySRPTScheduler::recomputeGlobalSrptSchedule()
{

    scheduledMsgs.clear();

    // The map below tracks the matches found in the scheduling algorithm. 
    // For every receiver transport (ie. hash key), the mapped value is the
    // scheduled message for that receiver trasport.
    std::unordered_map<MinimalTransport*, InflightMessage*> rxSchedules;
    rxSchedules.clear();

    // Iterate through all messages that are waiting for transmission based on
    // the SRPT policy and match the senders and receivers.
    for (auto mesgIt = txMsgSet.begin(); mesgIt != txMsgSet.end(); ++mesgIt) {
        if (scheduledMsgs.size() == senderMsgMap.size() ||
                rxSchedules.size() == receiverMsgMap.size()) {
            break;
        }

        InflightMessage* mesg = *mesgIt;
        MinimalTransport* mesgRxTrans = mesg->rxTransport;
        MinimalTransport* mesgSxTrans = mesg->sxTransport;

        if (scheduledMsgs.find(mesgSxTrans) != scheduledMsgs.end() ||
                rxSchedules.find(mesgRxTrans) != rxSchedules.end()) {
            // A shorter message has been already found for either sender or
            // receiver of this mesg therefore this message can't be scheduled.
            continue;
        }

        rxSchedules[mesgRxTrans] = mesg;
        scheduledMsgs[mesgSxTrans] = mesg;
    }

    // set newly found schedule at the senders to transmit corresponding mesgs
    for (auto schedTxMesg : scheduledMsgs) {
        schedTxMesg.first->refreshSchedule();
    }
}

// -- OracleGreedySRPTScheduler::InflightMessage ---

/**
 * Construct a global reprsentation of the message from application. This
 * representation is only relevant as long as the messgae is active in the
 * network.
 *
 * \param appMesg
 *      The application message that this instance of InflightMessage is
 *      constructed for.
 * \param oracleScheduler
 *      The central oracle srpt scheduler that manages the transmit scheduling
 *      of all messages in the network.
 */
OracleGreedySRPTScheduler::InflightMessage::InflightMessage(Rpc* appMesg,
        OracleGreedySRPTScheduler* oracleScheduler)
    : oracleScheduler(oracleScheduler)  
    , msgId(oracleScheduler->getNewMsgId())
    , msgByteLen(appMesg->getByteLength())
    , sxTransport(oracleScheduler->addrToTransport(appMesg->getSrcAddr()))
    , srcAddr(appMesg->getSrcAddr())
    , bytesToSend(appMesg->getByteLength())
    , nextBytesToSend(0)
    , rxTransport(oracleScheduler->addrToTransport(appMesg->getDestAddr()))
    , destAddr(appMesg->getDestAddr())
    , bytesToRecv(appMesg->getByteLength())
    , msgCreationTime(appMesg->getMsgCreationTime())
{}

/**
 * Destructor for InflightMessage
 */
OracleGreedySRPTScheduler::InflightMessage::~InflightMessage()
{}

/**
 * Calling this function, returns the next chunk of data to be transmitted for
 * the message. 
 *
 * \param numBytesToSend
 *      size of the chunk of data requested by the caller of this function
 * \return numBytesToSend
 *      the actual size of return chunk
 * \return
 *      index of the first byte of the returned chunk
 */
uint32_t
OracleGreedySRPTScheduler::InflightMessage::getNextChunkToSend(
    uint16_t &numBytesToSend)
{
    numBytesToSend =
        downCast<uint16_t>(std::min((uint32_t)numBytesToSend, bytesToSend));
    bytesToSend -= numBytesToSend;
    uint32_t offsetByte = nextBytesToSend;
    nextBytesToSend += numBytesToSend;
    return offsetByte;
}

/**
 * At the receiver side of the message, appends newly received data chunk to the
 * message.
 *
 * \param byteOffset 
 *      Index of the first byte of the received chunk in the range of 0 to
 *      msgByteLen-1.
 * \param chunkLen
 *      The byte of length of the data chunk to be appended to this message.
 * \return
 *      If the received chunk was the last piece of the message and the
 *      message is complete after this message, this method returns true.
 *      Otherwise, returns false.
 */
bool
OracleGreedySRPTScheduler::InflightMessage::appendRecvdChunk(
    uint32_t byteOffset, uint16_t chunkLen)
{
    bool msgComplete = false;
    bytesToRecv -= chunkLen;
    if (bytesToRecv <= 0) {
        msgComplete = true;
    }
    return msgComplete;
}


// -- OracleGreedySRPTScheduler::MesgChunk ---
/**
 * Constructor
 */
OracleGreedySRPTScheduler::MesgChunk::MesgChunk()
    : msgId(0)
    , lastChunk(false)
    , offsetByte(0)
    , chunkLen(0)
    , srcAddr()
    , destAddr()
{}
