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

#ifndef __HOMATRANSPORT_HOMATRANSPORT_H_
#define __HOMATRANSPORT_HOMATRANSPORT_H_

#include <unordered_map>
#include <queue>
#include <vector>
#include <list>
#include <omnetpp.h>

#include "inet/transportlayer/contract/udp/UDPSocket.h"
#include "application/AppMessage_m.h"
#include "transport/HomaPkt.h"
#include "transport/TrafficPacer.h"

/**
 * Impelements a grant based, receiver side congection control transport
 * protocol. 
 */
class HomaTransport : public cSimpleModule
{
  public:
    static const uint32_t ETHERNET_PREAMBLE_SIZE = 8;
    static const uint32_t ETHERNET_HDR_SIZE = 14; 
    static const uint32_t MAX_ETHERNET_PAYLOAD_BYTES = 1500;
    static const uint32_t MIN_ETHERNET_PAYLOAD_BYTES = 46;
    static const uint32_t IP_HEADER_SIZE = 20;
    static const uint32_t UDP_HEADER_SIZE = 8;
    static const uint32_t ETHERNET_CRC_SIZE = 4;
    static const uint32_t INTER_PKT_GAP = 12; 


    enum SelfMsgKind
    {
        START = 1,
        GRANT = 2,
        STOP  = 3
    };

    HomaTransport();
    ~HomaTransport();

    // Signals
    static simsignal_t msgsLeftToSendSignal;
    static simsignal_t bytesLeftToSendSignal;
    static simsignal_t outstandingGrantBytesSignal;
    static simsignal_t totalOutstandingBytesSignal;
 

    class SendController;
    class ReceiveScheduler;
    class CompareInboundMsg;


    class OutboundMessage
    {
      public:
        explicit OutboundMessage();
        explicit OutboundMessage(AppMessage* outMsg, 
                SendController* sxController, uint64_t msgId);
        explicit OutboundMessage(const OutboundMessage& outboundMsg);
        ~OutboundMessage();
        OutboundMessage& operator=(const OutboundMessage& other);
        int sendBytes(uint32_t numBytes);
        void sendRequestAndUnsched();

      protected:
        SendController* sxController;
        uint64_t msgId; 
        uint32_t bytesLeft;
        uint32_t nextByteToSend;

        uint32_t dataBytesInReq; // number of data bytes the req. pkt will carry.
        uint32_t unschedDataBytes; // number of unsched bytes sent after the req.
        inet::L3Address destAddr;
        inet::L3Address srcAddr;
        simtime_t msgCreationTime;

      private:
        void copy(const OutboundMessage &other);
        void sendUnsched();
        friend class SendController;
    };

    class SendController
    {
      public:
        SendController(HomaTransport* transport);
        ~SendController();
        void processSendMsgFromApp(AppMessage* msg);
        void processReceivedGrant(HomaPkt* rxPkt);
        uint32_t getReqDataBytes(AppMessage* sxMsg);
        uint32_t getUnschedBytes(AppMessage* sxMsg);
      protected:
        HomaTransport* transport;
        uint64_t bytesLeftToSend; //statistics
        uint64_t msgId;
        std::unordered_map<uint64_t, OutboundMessage> outboundMsgMap;
        friend class OutboundMessage;
    };

    class InboundMessage {
      public:
        explicit InboundMessage();
        explicit InboundMessage(const InboundMessage& other);
        explicit InboundMessage(HomaPkt* rxPkt, ReceiveScheduler* rxScheduler);
        ~InboundMessage();
         
      protected:
        ReceiveScheduler *rxScheduler;
        inet::L3Address srcAddr;
        inet::L3Address destAddr;
        uint64_t msgIdAtSender;
        
        // variables that track the message completion
        uint32_t bytesToGrant;
        uint32_t bytesToReceive;

        // states that keeps the general infor about the message
        uint32_t msgSize;
        uint16_t bytesInReq;
        uint16_t totalUnschedBytes;

        // Meta data for statistic collecting
        simtime_t msgCreationTime;
        
        friend class CompareInboundMsg;
        friend class ReceiveScheduler;

      protected:
        void copy(const InboundMessage& other);

        /**
         * add received bytes to the inboundMsg 
         */
        void fillinRxBytes(uint32_t byteStart, uint32_t byteEnd);
    };
 
    class CompareInboundMsg
    {
      public:
        CompareInboundMsg()
        {}

        bool operator()(const InboundMessage* msg1, const InboundMessage* msg2)
        {
            return msg1->bytesToGrant > msg2->bytesToGrant;
        }
    };

    class ReceiveScheduler
    {

      public:
        typedef std::priority_queue<InboundMessage*,
                std::vector<InboundMessage*>, CompareInboundMsg> PriorityQueue;
        typedef std::unordered_map<uint64_t, std::list<InboundMessage*>>
                InboundMsgsMap;

        ReceiveScheduler(HomaTransport* transport);
        ~ReceiveScheduler();
        void processReceivedRequest(HomaPkt* rxPkt);
        void processReceivedSchedData(HomaPkt* rxPkt);
        void processReceivedUnschedData(HomaPkt* rxPkt);
        void sendAndScheduleGrant();
        void initialize(uint32_t grantMaxBytes, uint32_t nicLinkSpeed,
                cMessage* grantTimer);
        
      protected:
        HomaTransport* transport;
        cMessage* grantTimer;
        TrafficPacer* trafficPacer;
        PriorityQueue inboundMsgQueue; 

        // keeps a map of all inboundMsgs from their msgId key. The msgId on
        // can be the same for different messages from different senders so the
        // value would be a list of all messages with the same msgId.
        InboundMsgsMap incompleteRxMsgs;
        uint32_t grantMaxBytes;

      protected:
        InboundMessage* lookupIncompleteRxMsg(HomaPkt* rxPkt);
        friend class HomaTransport;
        friend class InboundMessage;
        
    };


  protected:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
    virtual void finish();

    void sendPacket(HomaPkt* sxPkt);
    void processStart();
    void processGrantTimer();
    uint32_t getBytesOnWire(uint32_t numDataBytes, PktType homaPktType);

  protected:
    // contain the transport code in the send and receive paths.
    SendController sxController;
    ReceiveScheduler rxScheduler;

    // State
    inet::UDPSocket socket;
    cMessage* selfMsg;

    // parameters
    int localPort;
    int destPort;
    int nicLinkSpeed;
    int maxOutstandingRecvBytes;

    // this variable tracks the outstanding grant bytes. This in only for
    // getting statistics.
    int outstandingGrantBytes;

    friend class ReceiveScheduler;
};

#endif
