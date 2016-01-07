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
#include "transport/UnschedByteAllocator.h"

/**
 * A grant based, receiver driven, congection control transport protocol over
 * UDP datagram. For every message transmission, the sender side sends a req.
 * pkt defining the length of the message. The reciever on the other side send a
 * grant every packet time for the shortest remaining outstanding message among
 * all outstanding messages.
 */
class HomaTransport : public cSimpleModule
{
  public:
    /**
     * A self message essentially models a timer for this transport and can have
     * one the below types.
     */
    enum SelfMsgKind
    {
        START = 1,  // Timer type when the transport is in initialization phase.
        GRANT = 2,  // Timer type in normal working state of transport.
        STOP  = 3   // Timer type when the transport is in cleaning phase.
    };

    HomaTransport();
    ~HomaTransport();

    /**
     * C++ declration of signals defined in .ned file.
     */
    // Signal for number of incomplete TX msgs received from the application.
    static simsignal_t msgsLeftToSendSignal;

    // Signal for total numbet of msg bytes remained to send for all msgs.
    static simsignal_t bytesLeftToSendSignal;

    // Signal for total number of in flight grant bytes.
    static simsignal_t outstandingGrantBytesSignal;

    // Signal for total number of in flight bytes including both grants and
    // unscheduled packets.
    static simsignal_t totalOutstandingBytesSignal;
 
    class SendController;
    class ReceiveScheduler;
    class CompareInboundMsg;

    /**
     * Represents and handles transmiting a message from the senders side.
     * For each message represented by this class, this class exposes the api
     * for sending the req. pkt and some number of unscheduled pkts following
     * the request. This class also allows scheduled data to transmitted.
     */
    class OutboundMessage
    {
      public:
        explicit OutboundMessage();
        explicit OutboundMessage(AppMessage* outMsg, 
                SendController* sxController, uint64_t msgId,
                uint32_t dataBytesInReq, uint32_t unschedDataBytes);
        explicit OutboundMessage(const OutboundMessage& outboundMsg);
        ~OutboundMessage();
        OutboundMessage& operator=(const OutboundMessage& other);
        void sendRequestAndUnsched();
        int sendSchedBytes(uint32_t numBytes);

      protected:

        // The SendController that manages the transmission of this msg. 
        SendController* sxController;

        // Unique identification number assigned by in the construction time for
        // the purpose of easy external tracking of this message.
        uint64_t msgId; 

        // Total num bytes remained to be sent for this msg.
        uint32_t bytesLeft;

        // Index of the next byte to be transmitted for this msg. Always
        // initialized to zero.
        uint32_t nextByteToSend;
        
        // number of data bytes pibby backed in the req. pkt.
        uint32_t dataBytesInReq;

        // number of unsched bytes sent in unsched. pkts following the req pkt.
        uint32_t unschedDataBytes;

        // IpAddress of destination host for this outbound msg.
        inet::L3Address destAddr;

        // IpAddress of sender host (local host).
        inet::L3Address srcAddr;

        // Simulation global time at which this message was originally created
        // in the application.
        simtime_t msgCreationTime;

      private:
        void copy(const OutboundMessage &other);
        friend class SendController;
    };
    
    /**
     * Manages the transmission of all OutboundMessages from this transport and
     * keeps the state necessary for transmisssion of the messages. For every
     * new message that arrives from the applications, this class is responsible
     * for sending the request packet, unscheduled packets, and scheduled packet
     * (when a grants are received).
     */
    class SendController
    {
      public:
        SendController(HomaTransport* transport);
        ~SendController();
        void initSendController(uint32_t defaultReqBytes,
                uint32_t defaultUnschedBytes);
        void processSendMsgFromApp(AppMessage* msg);
        void processReceivedGrant(HomaPkt* rxPkt);

      protected:

        // Transport that owns this SendController.
        HomaTransport* transport;

        // For the purpose of statistics recording, this variable tracks the
        // total number bytes left to send over all outstanding messages. 
        uint64_t bytesLeftToSend;

        // The identification number for the next outstanding message.
        uint64_t msgId;

        // The hash map from the msgId to outstanding messages.
        std::unordered_map<uint64_t, OutboundMessage> outboundMsgMap;

        // For each distinct receiver, allocates the number of request and
        // unsched bytes for various sizes of message. 
        UnschedByteAllocator* unschedByteAllocator;
        friend class OutboundMessage;
    };

    /**
     * Handles reception of an incoming message by concatanations of data
     * fragments in received packets and keeping track of reception progress.
     */
    class InboundMessage {
      public:
        explicit InboundMessage();
        explicit InboundMessage(const InboundMessage& other);
        explicit InboundMessage(HomaPkt* rxPkt, ReceiveScheduler* rxScheduler);
        ~InboundMessage();
         
      protected:
        // The ReceiveScheduler that manages the reception of this message.
        ReceiveScheduler *rxScheduler;

        // Address of the sender of this message.
        inet::L3Address srcAddr;

        // Address of the receiver (ie. this host) of this message. Used to
        // specify the sources address when grant packets are being sent. 
        inet::L3Address destAddr;

        // The id of this message at the sender host. Used in the grant packets
        // to help the sender identify which outbound message a received grant
        // belongs to.
        uint64_t msgIdAtSender;
        
        // Tracks the total number of grant bytes that the rxScheduler should
        // send for this message. 
        uint32_t bytesToGrant;

        // Tracks the total number of bytes that has not yet been received for
        // this message. The message is complete when this value reaches zero
        // and therefore it can be handed over to the application.
        uint32_t bytesToReceive;

        // The total size of the message as indicated in the req. packet.
        uint32_t msgSize;

        // Number of data bytes carried over in the req. packets.
        uint16_t bytesInReq;

        // All unscheduled bytes that comes in req. pkts and following unsched.
        // packets for this message.
        uint16_t totalUnschedBytes;

        // simulation time at which this message was created in the sender side.
        // Used to calculate the end to end latency of this message.
        simtime_t msgCreationTime;

        // simulation time at which the first packet (req. pkt) of this inbound
        // message arrived at receiver. Used only for statistics recording
        // purpose.
        simtime_t reqArrivalTime;

        // When the last grant for this message was scheduled. Used only for
        // statistics recording purpose. Initialized in the constructor and must
        // only be updated by the prepareGrant() method.
        simtime_t lastGrantTime;

        friend class CompareInboundMsg;
        friend class ReceiveScheduler;

      protected:
        void copy(const InboundMessage& other);
        void fillinRxBytes(uint32_t byteStart, uint32_t byteEnd);
        HomaPkt* prepareGrant(uint32_t grantSize);
        AppMessage* prepareRxMsgForApp();
        
    };
 
    /**
     * Manages reception of messages that are being sent to this host through
     * this transport. Keeps a list of all incomplete rx messages and sends
     * grants for them based on SRPT policy. At the completion of each message,
     * it will be handed off to the application. 
     */
    class ReceiveScheduler
    {
      public:

        enum QueueType
        {
            PRIO_QUEUE = 0,
            FIFO_QUEUE = 1,
            INVALID_TYPE = 2
        };

        class InboundMsgQueue 
        {
          public:
            
            /**
             * A predicate functor that compares the remaining required grants
             * to be sent for two inbound message.
             */
            class CompareInboundMsg
            {
              public:
                CompareInboundMsg()
                {}
                
                /**
                 * Predicate functor operator () for comparison.
                 *
                 * \param msg1
                 *      inbound message 1 in the comparison
                 * \param msg2
                 *      inbound message 2 in the comparison
                 * \return
                 *      a bool from the result of the comparison
                 */
                bool operator()(const InboundMessage* msg1,
                    const InboundMessage* msg2)
                {
                    return msg1->bytesToGrant > msg2->bytesToGrant;
                }
            };

            typedef std::priority_queue<InboundMessage*,
                    std::vector<InboundMessage*>, CompareInboundMsg> PriorityQueue;

            explicit InboundMsgQueue();
            void initialize (QueueType queueType);
            void push(InboundMessage* inboundMsg);
            InboundMessage* top();
            void pop();
            bool empty();
            size_t size();

          private:
            PriorityQueue prioQueue; 
            std::queue<InboundMessage*> fifoQueue;
            QueueType queueType;    
        };

        typedef std::unordered_map<uint64_t, std::list<InboundMessage*>>
                InboundMsgsMap;

        ReceiveScheduler(HomaTransport* transport);
        ~ReceiveScheduler();
        void processReceivedRequest(HomaPkt* rxPkt);
        void processReceivedSchedData(HomaPkt* rxPkt);
        void processReceivedUnschedData(HomaPkt* rxPkt);
        void sendAndScheduleGrant();
        void initialize(uint32_t grantMaxBytes, uint32_t nicLinkSpeed,
                cMessage* grantTimer, QueueType queueType);
        
      protected:
        HomaTransport* transport;
        cMessage* grantTimer;
        TrafficPacer* trafficPacer;

        // A container for incomplete messages that are sorted based on the
        // remaining bytes to grant.
        InboundMsgQueue inboundMsgQueue; 

        // Keeps a hash map of all incomplete inboundMsgs from their msgId key.
        // the value of the map is list of all messages with the same msgId from
        // different senders
        InboundMsgsMap incompleteRxMsgs;

        // The upper bound on the number of grant data byte carried sent in
        // individual grant packets .
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

  protected:

    // Handles the transmission of outbound messages based on the logic of
    // HomaProtocol. 
    SendController sxController;

    // Manages the reception of all inbound messages.
    ReceiveScheduler rxScheduler;

    // UDP socket through which this transport send and receive packets.
    inet::UDPSocket socket;

    // Timer object for this transport. Will be used for implementing timely
    // scheduled  
    cMessage* selfMsg;

    // udp ports assigned to this transprt
    int localPort;
    int destPort;

    // NIC link speed (in Gb/s) connected to this host. This parameter will be
    // read from the omnetpp.ini config file. 
    int nicLinkSpeed;

    // This parameter is read from the omnetpp.ini config file and provides an
    // upper bound on the total allowed outstanding bytes. It is necessary (BUT
    // NOT ENOUGH) for the rxScheduler to check that the total outstanding bytes
    // is smaller than this value every time a new grant is to be sent.
    int maxOutstandingRecvBytes;

    // Tracks the total outstanding grant bytes which will be used for stats
    // collection and recording.
    int outstandingGrantBytes;

    friend class ReceiveScheduler;
};

#endif
