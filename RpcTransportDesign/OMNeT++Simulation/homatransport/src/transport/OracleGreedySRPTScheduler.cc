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

Define_Module(OracleGreedySRPTScheduler);

OracleGreedySRPTScheduler::OracleGreedySRPTScheduler()
    : globalMsgId(0) 
    , senderMsgMap()
    , receiverMsgMap()
    , hostTranports()
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
    return;
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
 *      
 * \return
 *      False if transport has been previously subscribed and therefor can't be
 *      subscribed again.
 */
bool
OracleGreedySRPTScheduler::subscribeEndhostTransport(
    MinimalTransport* transport, inet::L3Address transportAddr)
{
    try {
        senderMsgMap.at(transport);
    } catch (std::exception& e) {
        hostTranports.push_back(transport);
        senderMsgMap[transport] = {};
        receiverMsgMap[transport] = {};
        ASSERT(transportAddr.getType() == inet::L3Address::AddressType::IPv4);
        uint32_t ipv4Addr = transportAddr.toIPv4().getInt();
        hostAddrTransports[ipv4Addr] = transport;
        return true;
    }
    return false;
}

/**
 * This function must be called whenever a new message has entered the
 * simulation from an application at the end host. This method informs the
 * oracle scheduler about the new created message and the message will be
 * cosidered in the scheduling algorithm in the future.
 *
 * \param appMesg
 *      pointer to the message created in application that needs to be
 *      transmitted over the network.
 */
void
OracleGreedySRPTScheduler::scheduleNewMesg(AppMessage* appMesg)
{
    InflightMessage* newMsg = new InflightMessage(appMesg, this);
    senderMsgMap.at(newMsg->sxTransport).insert(newMsg);
    receiverMsgMap.at(newMsg->rxTransport).insert(newMsg);
    msgIdMap[newMsg->msgId] = newMsg;
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
 *
 * \return
 *      Pointer to chunk of the scheduled message to be transmitted by
 *      sxTransport. Null means no message scheduled for sxTransport. When not
 *      null, caller is responsible for allocated memory for the returned chunk.
 */
OracleGreedySRPTScheduler::MesgChunk*
OracleGreedySRPTScheduler::getNextChunkToSend(MinimalTransport* sxTransport, uint16_t chunkSize)
{
    InflightMessage* msg = NULL;
    try {
        msg = scheduledMsgs.at(sxTransport);
    } catch(std::exception& e) {
        return NULL;
    }

    // Side effect: Calling this function reduces the remaining bytes of the
    // messages and therefore might change the position of this message in
    // senderMsgMap and receiverMsgMap. These changes must be appropriately
    // applied in senderMsgMap and receiverMsgMap.
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
        recomputeGlobalSrptSchedule();
    } else {
        allSxMsgs.insert(msg);
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
 *
 * \return
 *      If the received chunk was the last piece of the message and the
 *      message is complete after this message, this method constructs an
 *      AppMessage corresponding to this message and returns the pointer to the
 *      AppMessage. Otherwise, it returns null.
 *      Note: It is the responsibility of the caller to free up the allocated
 *      memory for the returned AppMessage.
 */
AppMessage*
OracleGreedySRPTScheduler::appendRecvdChunk(
    OracleGreedySRPTScheduler::MesgChunk* msgChunk)
{
    AppMessage* rxMsg = NULL;
    InflightMessage* msg = msgIdMap.at(msgChunk->msgId);
    if (msg->appendRecvdChunk(msgChunk->offsetByte, msgChunk->chunkLen)) {
        msgChunk->lastChunk = true;
        msgIdMap.erase(msgChunk->msgId);
        auto& allRxMsg = receiverMsgMap.at(msg->rxTransport);
        allRxMsg.erase(msg);
        auto& allSxMsgs = senderMsgMap.at(msg->sxTransport);
        ASSERT(allSxMsgs.find(msg) == allSxMsgs.end());
        rxMsg = new AppMessage();
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
    // The key is end host transport at sender side, the value is a pair.
    // pair.first is the mesg schedule found for the key transport as a sender
    // and pair.second is the index of outstanding sx messages that has been
    // inpected in the set of all messages for the key transport.
    std::unordered_map<MinimalTransport*, std::pair<InflightMessage*, size_t>>
        sxSchedules;

    // The map below tracks the matches found in the scheduling algorithm. 
    // The key is end host transport at receiver side, the value is a sorted set
    // of possible schedules used in every iteration of the scheduling
    // algorithm until a match is found.
    std::unordered_map<MinimalTransport*, std::set<InflightMessage*,
        InflightMessage::MesgSorter>> rxPotentialSchedules;
    
    // For every receiver transport (key), the map will contain the scheduled
    // message (value) for that trasport.
    std::unordered_map<MinimalTransport*, InflightMessage*> rxSchedules;

    do {
        rxPotentialSchedules.clear();
        size_t randOffset = intrand(hostTranports.size());
        for (size_t i = 0; i < hostTranports.size(); i++) {
            size_t ind = (i + randOffset) % hostTranports.size();
            MinimalTransport* sxTransport = hostTranports[ind];
            auto sxSchedule = sxSchedules[sxTransport];
            if (sxSchedule.first != NULL) {
                // match has been found
                continue;
            }

            auto sxMsgsSorted = senderMsgMap[sxTransport];
            if (sxSchedule.second >= sxMsgsSorted.size()) {
                // This all outstaanding mesgs of this sender transport has been
                // tried in previous iterations and couldn't find a match for
                // them.
                continue;
            }

            auto mesgToSched = sxMsgsSorted.begin(); 
            std::advance(mesgToSched, sxSchedule.second);
            while (sxSchedule.second < sxMsgsSorted.size()) {
                sxSchedule.second++;
                MinimalTransport* msgRxTrans = (*mesgToSched)->rxTransport;
                auto schedMsg = rxSchedules[msgRxTrans];
                if (schedMsg) {
                    mesgToSched++;
                    continue;
                } else {
                    rxPotentialSchedules[msgRxTrans].insert(schedMsg);
                    break;
                }
            }
        }

        for (auto schedsIt : rxPotentialSchedules) {
            MinimalTransport* rxTransport = schedsIt.first;
            auto potentialScheds = schedsIt.second; 
            ASSERT(rxSchedules[rxTransport] == NULL);
            InflightMessage* schedMsg = *(potentialScheds.begin());
            ASSERT(sxSchedules[schedMsg->sxTransport].first == NULL &&
                rxSchedules[schedMsg->rxTransport] == NULL);
            sxSchedules[schedMsg->sxTransport].first = schedMsg;
            scheduledMsgs[schedMsg->sxTransport] = schedMsg;
            rxSchedules[schedMsg->rxTransport] = schedMsg;
        }
    } while (!rxPotentialSchedules.empty());

    // set newly found schedule at the senders to transmit corresponding mesgs
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
OracleGreedySRPTScheduler::InflightMessage::InflightMessage(AppMessage* appMesg,
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
 *
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
