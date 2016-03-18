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

#ifndef __HOMATRANSPORT_ORACLEGREEDYSRPTSCHEDULER_H_
#define __HOMATRANSPORT_ORACLEGREEDYSRPTSCHEDULER_H_

#include <omnetpp.h>
#include <unordered_map>
#include <unordered_set>
#include "common/Minimal.h"
#include "inet/transportlayer/contract/udp/UDPSocket.h"
#include "application/Rpc_m.h"


class MinimalTransport;
/**
 * Implements a centralized online scheduler that uses iterative greedy
 * algorithm to find a maximal message transmission schedule based on SRPT
 * policy.
 */
class OracleGreedySRPTScheduler : public cSimpleModule
{
  PUBLIC:
    /**
     * Exposes a chunk of InflightMessage to the sender and receiver end host
     * transports. Provides enough information for the end host transports to
     * send and receive a piece of message.
     */
    class MesgChunk
    {
      PUBLIC:
        explicit MesgChunk();
        ~MesgChunk(){}

      PUBLIC:
        // The global Id of the message this chunk belongs to in simulation.
        uint64_t msgId;

        // True if this is the last chunk of the message
        bool lastChunk;

        // The offset byte index of this chunk in the range of 0 to msgByteLen-1
        uint64_t offsetByte;

        // Total bytes transmitted in this chunk
        uint32_t chunkLen;

        // source and destinatio of the message this chunk belong to.
        inet::L3Address srcAddr;
        inet::L3Address destAddr;
    };

  PUBLIC:
    OracleGreedySRPTScheduler();
    ~OracleGreedySRPTScheduler();
    bool subscribeEndhostTransport(MinimalTransport* transport,
        inet::L3Address transportAddr);
    void scheduleNewMesg(Rpc* appMesg);
    MesgChunk* getNextChunkToSend(MinimalTransport* sxTransport,
        uint16_t chunkSize);
    Rpc* appendRecvdChunk(MesgChunk* msgChunk);


  PROTECTED:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
    virtual void finish();
    uint64_t getNewMsgId();
    MinimalTransport* addrToTransport(inet::L3Address ipAddr);

    /**
     * Defines a global representation of a message that is being transmitted
     * over the network.
     */
    class InflightMessage
    {
      PUBLIC:
        explicit InflightMessage(Rpc* msgToSend,
            OracleGreedySRPTScheduler *oracleScheduler);
        ~InflightMessage();

        /**
         * This class is a function object that compare remaining bytes and size of
         * two message and enforces a weak ordering among them. This class
         * satisfies the requirements of std::Compare.
         */
        class MesgSorter {
          PUBLIC:
            MesgSorter() {};
            bool operator()(const InflightMessage* lhs,
                const InflightMessage* rhs)
            {
                return lhs->bytesToSend < rhs->bytesToSend ||
                    (lhs->bytesToSend == rhs->bytesToSend && 
                    lhs->msgByteLen < rhs->msgByteLen) ||
                    (lhs->bytesToSend == rhs->bytesToSend && 
                    lhs->msgByteLen == rhs->msgByteLen &&
                    lhs->msgCreationTime < rhs->msgCreationTime); 
            }
        };

      PUBLIC:
        // pointer to the OracleGreedySRPTScheduler that schedules this message
        // for transmission
        OracleGreedySRPTScheduler* oracleScheduler;

        // The global id of this message through out the whole simulation domain
        uint64_t msgId;

        // Total data bytes of this message
        uint32_t msgByteLen;

        //** Variables associated to the sender side **//
        MinimalTransport* sxTransport;
        inet::L3Address srcAddr;

        // total number of bytes left to be transmitted at sender
        uint32_t bytesToSend;
        
        // tracks the index of first bytes of the next chunk to be transmitted 
        uint32_t nextBytesToSend;

        //** Variables associated to the receiver side **//
        MinimalTransport* rxTransport;
        inet::L3Address destAddr;
        uint32_t bytesToRecv;
        simtime_t msgCreationTime;
        friend class OracleGreedySRPTScheduler;

      PROTECTED:
        // Set of methods needed for sending data at sender node
        uint32_t getNextChunkToSend(uint16_t &numBytesToSend);
        
        // Set of methods requiered in the receiving path of the message
        bool appendRecvdChunk(uint32_t byteOffset, uint16_t chunkLen);
    };

  PROTECTED:
    typedef std::unordered_map<MinimalTransport*,
        std::set<InflightMessage*, InflightMessage::MesgSorter>> EndhostMesgMap;

    // A counter that monotonically increasese and tracks the global
    // identification number of next message to be generated in the simulation.
    uint64_t globalMsgId;

    // A hash table from sender/receiver transports to a sorted list of all
    // active messages associated with that sender/reciever.
    EndhostMesgMap senderMsgMap;
    EndhostMesgMap receiverMsgMap;

    // contains pointer to all end host transports.
    std::vector<MinimalTransport*> hostTranports;

    // maps msgIds to pointer to InflightMessage
    std::unordered_map<uint64_t, InflightMessage*> msgIdMap;

    // IPv4 Address to host transport map
    std::unordered_map<uint32_t, MinimalTransport*> hostAddrTransports;

    // Contains the most recent scheduled messages for transmissio at senders
    std::unordered_map<MinimalTransport*, InflightMessage*> scheduledMsgs;

    friend class MinimalTransport;
    friend class InflightMessage;

  PRIVATE:
    void recomputeGlobalSrptSchedule();
};

#endif //end __HOMATRANSPORT_ORACLEGREEDYSRPTSCHEDULER_H_
