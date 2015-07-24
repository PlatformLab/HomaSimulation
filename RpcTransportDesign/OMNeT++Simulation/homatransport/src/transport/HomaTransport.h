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
#include "transport/HomaPkt_m.h"
#include "transport/ByteBucket.h"

/**
 * Impelements a grant based, receiver side congection control transport
 * protocol. 
 */
class HomaTransport : public cSimpleModule
{
  public:
    enum SelfMsgKind
    {
        START = 1,
        GRANT = 2,
        STOP  = 3
    };

    HomaTransport();
    ~HomaTransport();

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
        void sendRequest(simtime_t msgCreationTime);

      protected:
        SendController* sxController;
        uint32_t bytesLeft;
        uint32_t nextByteToSend;
        inet::L3Address destAddr;
        inet::L3Address srcAddr;
        uint64_t msgId; 

      private:
        void copy(const OutboundMessage &other);  
        friend class SendController;
    };

    class SendController
    {
      public:
        SendController(HomaTransport* transport);
        ~SendController();
        void processSendMsgFromApp(AppMessage* msg);
        void processReceivedGrant(HomaPkt* rxPkt);

      protected:
        HomaTransport* transport;
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

        uint32_t bytesToGrant;
        uint32_t bytesToReceive;
        uint32_t msgSize;

        // Meta data for statistic collecting
        simtime_t msgCreationTime;
        
        friend class CompareInboundMsg;
        friend class ReceiveScheduler;

      protected:
        void copy(const InboundMessage& other);
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

        ReceiveScheduler(HomaTransport* transport);
        ~ReceiveScheduler();
        void processReceivedRequest(HomaPkt* rxPkt);
        void processReceivedData(HomaPkt* rxPkt);
        void sendAndScheduleGrant();
        void initialize(uint32_t grantMaxBytes, uint32_t nicLinkSpeed, cMessage* grantTimer);
        
      protected:
        HomaTransport* transport;
        cMessage* grantTimer;
        ByteBucket* byteBucket;
        PriorityQueue inboundMsgQueue; 
        std::list<InboundMessage*> incompleteRxMsgs;
        uint32_t grantMaxBytes;
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
    double grantTimeInterval;

    friend class ReceiveScheduler;
};

#endif
