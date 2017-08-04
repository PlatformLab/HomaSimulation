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

#include <queue>
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <omnetpp.h>
#include "common/Minimal.h"
#include "application/AppMessage_m.h"
#include "transport/HomaPkt.h"
#include "transport/UnschedByteAllocator.h"
#include "transport/WorkloadEstimator.h"
#include "common/Util.h"
#include "transport/HomaConfigDepot.h"
#include "mocks/MockUdpSocket.h"
#include "inet/transportlayer/contract/udp/UDPSocket.h"

#if TESTING
typedef MockUdpSocket UDPSocket;
#else
typedef inet::UDPSocket UDPSocket;
#endif

// forward decalration to enable mutual header include
class TrafficPacer;
class PriorityResolver;

/**
 * A grant based, receiver driven, congection control transport protocol over
 * UDP datagrams. For every message transmission, the sender side sends a req.
 * pkt defining the length of the message. The reciever on the other side sends
 * a grant every packet time for the shortest remaining outstanding message
 * among all outstanding messages.
 */
class HomaTransport : public cSimpleModule
{
  PUBLIC:
    HomaTransport();
    ~HomaTransport();

    class SendController;
    class ReceiveScheduler;

    /**
     * Represents and handles transmiting a message from the senders side.
     * For each message represented by this class, this class exposes the api
     * for sending the req. pkt and some number of unscheduled pkts following
     * the request. This class also allows scheduled data to transmitted.
     */
    class OutboundMessage
    {
      PUBLIC:
        explicit OutboundMessage();
        explicit OutboundMessage(AppMessage* outMsg,
                SendController* sxController, uint64_t msgId,
                std::vector<uint16_t> reqUnschedDataVec);
        explicit OutboundMessage(const OutboundMessage& outboundMsg);
        ~OutboundMessage();
        OutboundMessage& operator=(const OutboundMessage& other);
        void prepareRequestAndUnsched();
        uint32_t prepareSchedPkt(uint32_t offset, uint16_t numBytes,
            uint16_t schedPrio);

      PUBLIC:
        /**
         * A utility predicate for creating PriorityQueues of HomaPkt instances.
         * Determines in what order pkts of this msg will be sent out to the
         * network.
         */
        class OutbndPktSorter {
          PUBLIC:
            OutbndPktSorter(){}
            bool operator()(const HomaPkt* pkt1, const HomaPkt* pkt2);
        };

        typedef std::priority_queue<
            HomaPkt*,
            std::vector<HomaPkt*>,
            OutbndPktSorter> OutbndPktQueue;

        // getters functions
        const uint32_t& getMsgSize() { return msgSize; }
        const uint64_t& getMsgId() { return msgId; }
        const OutbndPktQueue& getTxPktQueue() {return txPkts;}
        const std::unordered_set<HomaPkt*>& getTxSchedPkts()
            {return txSchedPkts;}
        const uint32_t getBytesLeft() { return bytesLeft; }
        const simtime_t getMsgCreationTime() { return msgCreationTime; }
        bool getTransmitReadyPkt(HomaPkt** outPkt);

      PROTECTED:
        // Unique identification number assigned by in the construction time for
        // the purpose of easy external access to this message.
        uint64_t msgId;

        // Total byte size of the message received from application
        uint32_t msgSize;

        // Total num bytes that need grants from the receiver for transmission
        // in this mesg.
        uint32_t bytesToSched;

        // Total num bytes left to be transmitted for this messge, including
        // bytes that are packetized and queued in the transport, but not yet
        // sent to the network interface for transmission.
        uint32_t bytesLeft;

        // Total unsched bytes left to be transmitted for this messge.
        uint32_t unschedBytesLeft;

        // Next time this message expects an unsched packet must be sent. This
        // is equal to time at which last bit of last unsched pkt was serialized
        // out to the network.
        simtime_t nextExpectedUnschedTxTime;

        // This vector is length of total unsched pkts to be sent for
        // this message. Element i in this vector is number of data bytes to be
        // sent by the i'th unsched packet for this message. Note that first
        // unsched pkt at position zero of this vector is always the request
        // packet and sum of the elements is the total unsched bytes sent for
        // this mesage. For every message, there will at least one unsched
        // packet that is request packet, so this vector is at least size 1.
        std::vector<uint16_t> reqUnschedDataVec;

        // IpAddress of destination host for this outbound msg.
        inet::L3Address destAddr;

        // IpAddress of sender host (local host).
        inet::L3Address srcAddr;

        // Simulation global time at which this message was originally created
        // in the application.
        simtime_t msgCreationTime;

        // Priority Queue containing all sched/unsched pkts are to be sent out
        // for this message and are waiting for transmission.
        OutbndPktQueue txPkts;

        // Set of all sched pkts ready for transmission
        std::unordered_set<HomaPkt*> txSchedPkts;

        // The SendController that manages the transmission of this msg.
        SendController* sxController;

        // The object that keeps all transport configuration parameters
        HomaConfigDepot *homaConfig;

      PRIVATE:
        void copy(const OutboundMessage &other);
        std::vector<uint32_t> getUnschedPerPrio(
            std::vector<uint32_t>& unschedPrioVec);
        friend class SendController;
        friend class PriorityResolver;
    };

    /**
     * Manages the transmission of all OutboundMessages from this transport and
     * keeps the state necessary for transmisssion of the messages. For every
     * new message that arrives from the applications, this class is responsible
     * for sending the request packet, unscheduled packets, and scheduled packet
     * (when grants are received).
     */
    class SendController
    {
      PUBLIC:
        typedef std::unordered_map<uint64_t, OutboundMessage> OutboundMsgMap;
        SendController(HomaTransport* transport);
        ~SendController();
        void initSendController(HomaConfigDepot* homaConfig,
            PriorityResolver* prioResolver);
        void processSendMsgFromApp(AppMessage* msg);
        void processReceivedGrant(HomaPkt* rxPkt);
        OutboundMsgMap* getOutboundMsgMap() {return &outboundMsgMap;}
        void sendOrQueue(cMessage* msg = NULL);
        void handlePktTransmitEnd();

      PUBLIC:
        /**
         * A predicate func object for sorting OutboundMessages based on the
         * senderScheme used for transport. Detemines in what order
         * OutboundMessages will send their sched/unsched outbound packets (ie.
         * pkts queues in OutboundMessage::txPkts).
         */
        class OutbndMsgSorter {
          PUBLIC:
            OutbndMsgSorter(){}
            bool operator()(OutboundMessage* msg1, OutboundMessage* msg2);
        };
        typedef std::set<OutboundMessage*, OutbndMsgSorter> SortedOutboundMsg;

      PROTECTED:
        void sendPktAndScheduleNext(HomaPkt* sxPkt);
        void msgTransmitComplete(OutboundMessage* msg);

      PROTECTED:
        // For the purpose of statistics recording, this variable tracks the
        // total number bytes left to be sent for all outstanding messages.
        // (including unsched/sched bytes ready to be sent but not yet
        // transmitted).
        uint64_t bytesLeftToSend;

        // Sum of all sched bytes in all outbound messages that are yet to be
        // scheduled by their corresponding reveivers;
        uint64_t bytesNeedGrant;

        // The identification number for the next outstanding message.
        uint64_t msgId;

        // Keeps a copy of last packet transmitted by this module
        HomaPkt sentPkt;

        // Serialization time of sentPkt at nic link speed
        simtime_t sentPktDuration;

        // The hash map from the msgId to outstanding messages.
        OutboundMsgMap outboundMsgMap;

        // Each entry (ie. map value) is a sorted set of all outbound
        // messages for a specific receiver mapped to the ip address of that
        // receiver (ie. map key).
        std::unordered_map<uint32_t, std::unordered_set<OutboundMessage*>>
                rxAddrMsgMap;

        // For each distinct receiver, allocates the number of request and
        // unsched bytes for various sizes of message.
        UnschedByteAllocator* unschedByteAllocator;

        // Determine priority of packets that are to be sent
        PriorityResolver *prioResolver;

        // Sorted collection of all OutboundMessages that have unsched/sched
        // pkts queued up and ready to be sent out into the network.
        SortedOutboundMsg outbndMsgSet;

        // Queue for keeping grants that receiver side of this host machine
        // wants to send out.
        std::priority_queue<HomaPkt*, std::vector<HomaPkt*>,
            HomaPkt::HomaPktSorter> outGrantQueue;

        // Transport that owns this SendController.
        HomaTransport* transport;

        // The object that keeps the configuration parameters for the transport
        HomaConfigDepot *homaConfig;

        // Tracks the begining of time periods during which outstanding bytes is
        // nonzero.
        simtime_t activePeriodStart;

        // Tracks the bytes received during each active period.
        uint64_t sentBytesPerActivePeriod;

        // Tracks sum of grant pkts received between two respective received
        // data pkts.  SendController cumulates grant packets to this variable
        // and ReceiveScheduler zeros it out whenever a new data packet has
        // arrived.
        uint64_t sumGrantsInGap;

        friend class OutboundMessage;
        friend class HomaTransport;
    }; // End SendController

    /**
     * Handles reception of an incoming message by concatanations of data
     * fragments in received packets and keeping track of reception progress.
     */
    class InboundMessage
    {
      PUBLIC:
        explicit InboundMessage();
        explicit InboundMessage(const InboundMessage& other);
        explicit InboundMessage(HomaPkt* rxPkt, ReceiveScheduler* rxScheduler,
            HomaConfigDepot* homaConfig);
        ~InboundMessage();

      PUBLIC:
        typedef std::list<std::tuple<uint32_t, uint16_t, simtime_t>> GrantList;

        /**
         * A predicate functor that compares the remaining required grants
         * to be sent for two inbound message.
         */
        class CompareBytesToGrant
        {
          PUBLIC:
            CompareBytesToGrant()
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
                return (msg1->bytesToGrant < msg2->bytesToGrant) ||
                    (msg1->bytesToGrant == msg2->bytesToGrant &&
                        msg1->msgSize < msg2->msgSize) ||
                    (msg1->bytesToGrant == msg2->bytesToGrant &&
                        msg1->msgSize == msg2->msgSize &&
                        msg1->msgCreationTime < msg2->msgCreationTime) ||
                    (msg1->bytesToGrant == msg2->bytesToGrant &&
                        msg1->msgSize == msg2->msgSize &&
                        msg1->msgCreationTime == msg2->msgCreationTime &&
                        msg1->msgIdAtSender < msg2->msgIdAtSender);
            }
        };
        const uint32_t& getMsgSize() { return msgSize; }

      PROTECTED:
        // The ReceiveScheduler that manages the reception of this message.
        ReceiveScheduler *rxScheduler;

        // The object that keeps all transport configuration parameters
        HomaConfigDepot *homaConfig;

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

        // Tracks the next for this messaage that is to be scheduled for
        // transmission by next grant packet. This value is monotonically
        // increasing and together with next grantSize, uinquely identifies the
        // nest chunck of data bytes in the message will be tranmitted by the
        // sender.
        uint32_t offset;

        // Tracks the total number of data bytes scheduled (granted) for this
        // messages but has not yet been received.
        uint32_t bytesGrantedInFlight;

        // Total number of bytes inflight for this message including all
        // different header bytes and ethernet overhead bytes.
        uint32_t totalBytesInFlight;

        // Tracks the total number of bytes that has not yet been received for
        // this message. The message is complete when this value reaches zero
        // and therefore it can be handed over to the application.
        uint32_t bytesToReceive;

        // The total size of the message as indicated in the req. packet.
        uint32_t msgSize;

        // Tracks the number of scheduled data bytes plus header bytes, as the
        // receiver sends grants for this message.
        uint32_t schedBytesOnWire;

        // Total bytes transmitted on wire for this message
        uint32_t totalBytesOnWire;

        // All unscheduled bytes that come in req. pkt and the following
        // unsched packets for this message.
        uint16_t totalUnschedBytes;

        // simulation time at which this message was created in the sender side.
        // Used to calculate the end to end latency of this message.
        simtime_t msgCreationTime;

        // simulation time at which the first packet (req. pkt) of this inbound
        // message arrived at receiver. Used only for statistics recording
        // purpose.
        simtime_t reqArrivalTime;

        //****************************************************************//
        //*****Below variables are for statistic collection purpose.******//
        //****************************************************************//
        // When the last grant for this message was scheduled. Initialized in
        // the constructor and must only be updated by the prepareGrant()
        // method.
        simtime_t lastGrantTime;

        // List to keep track of the outstanding grant pkts. Each tuple in the
        // list has the offset byte scheduled by a grant, and the size of the
        // grant and time at which the receiver scheduled that grant. The
        // unscheduled bytes are also added to this list as hypothetical grant
        // pkts that receiver sent one RTT before the first packet arrived and
        // for zero offset byte.
        GrantList inflightGrants;

        //***************************************************************//
        //****Below variables are snapshots, first after construction****//
        //****and then at grant times, of the corresponding variables****//
        //****defined in ReceiveScheduler.                           ****//
        //***************************************************************//
        std::vector<uint64_t> bytesRecvdPerPrio;
        std::vector<uint64_t> scheduledBytesPerPrio;
        std::vector<uint64_t> unschedToRecvPerPrio;

        //***************************************************************//
        //****Below variables are snapshots, first after construction****//
        //****and then at grant times, of the corresponding variables****//
        //****defined in TrafficPacer.                               ****//
        //***************************************************************//
        std::vector<uint32_t> sumInflightUnschedPerPrio;
        std::vector<uint32_t> sumInflightSchedPerPrio;

        friend class CompareBytesToGrant;
        friend class ReceiveScheduler;
        friend class TrafficPacer;
        friend class PriorityResolver;

      PROTECTED:
        void copy(const InboundMessage& other);
        void fillinRxBytes(uint32_t byteStart, uint32_t byteEnd,
            PktType pktType);
        uint32_t schedBytesInFlight();
        uint32_t unschedBytesInFlight();
        HomaPkt* prepareGrant(uint16_t grantSize, uint16_t schedPrio);
        AppMessage* prepareRxMsgForApp();
        void updatePerPrioStats();
    };

    /**
     * Manages reception of messages that are being sent to this host through
     * this transport. Keeps a list of all incomplete rx messages and sends
     * grants for them based on SRPT policy. At the completion of each message,
     * it will be handed off to the application.
     */
    class ReceiveScheduler
    {
      PUBLIC:
        ReceiveScheduler(HomaTransport* transport);
        ~ReceiveScheduler();
        InboundMessage* lookupInboundMesg(HomaPkt* rxPkt) const;

        class UnschedRateComputer {
          PUBLIC:
            UnschedRateComputer(HomaConfigDepot* homaConfig,
                bool computeAvgUnschRate = false, double minAvgTimeWindow = .1);
            double getAvgUnschRate(simtime_t currentTime);
            void updateUnschRate(simtime_t arrivalTime, uint32_t bytesRecvd);

          PUBLIC:
            bool computeAvgUnschRate;
            std::vector<std::pair<uint32_t, double>> bytesRecvTime;
            uint64_t sumBytes;
            double minAvgTimeWindow; // in seconds
            HomaConfigDepot* homaConfig;
        };

        /**
         * Per sender object to manage the reception of messages from each
         * sender. It also contains all per sender information that are needed
         * to schedule messages of the corresponding sender.
         */
        class SenderState {
          PUBLIC:
            SenderState(inet::IPv4Address srcAddr,
                ReceiveScheduler* rxScheduler, cMessage* grantTimer,
                HomaConfigDepot* homaConfig);
            ~SenderState(){}
            const inet::IPv4Address& getSenderAddr() {
                return senderAddr;
            }
            simtime_t getNextGrantTime(simtime_t currentTime,
                uint32_t grantSize);
            int sendAndScheduleGrant(uint32_t grantPrio);
            std::pair<bool, int> handleInboundPkt(HomaPkt* rxPkt);

          PROTECTED:
            HomaConfigDepot* homaConfig;
            ReceiveScheduler* rxScheduler;
            inet::IPv4Address senderAddr;

            // Timer object for sending grants for this sender. Will be used
            // to send timer paced grants for this sender if totalBytesInFlight
            // for the top mesg of this sender is less than RTT.
            cMessage* grantTimer;

            // A sorted list of sched messages that need grants from the sender.
            std::set<InboundMessage*, InboundMessage::CompareBytesToGrant>
                mesgsToGrant;

            // Map of all incomplete inboundMsgs from the sender hashed by msgId
            std::unordered_map<uint64_t, InboundMessage*> incompleteMesgs;

            // Priority of last sent grant for this sender
            uint32_t lastGrantPrio;

            // Index of this sender in SchedSenders as of the last time its been
            // removed from SchedSender.
            uint32_t lastIdx;

            friend class HomaTransport::ReceiveScheduler;
        };

        /**
         * Collection of all scheduled senders (ie. senders with at least one
         * message that needs grants). This object also contains all information
         * requiered for scheduling messages and implementing the scheduler's
         * logic and the sched prio assignment.
         */
        class SchedSenders {
          PUBLIC:
            SchedSenders(HomaConfigDepot* homaConfig, HomaTransport* transport,
                ReceiveScheduler* rxScheduler);
            ~SchedSenders() {}
            class CompSched {
              PUBLIC:
                CompSched(){}
                bool operator()(const SenderState* lhs, const SenderState* rhs)
                {
                    if (!lhs && !rhs)
                        return false;
                    if (!lhs && rhs)
                        return true;
                    if (lhs && !rhs)
                        return false;

                    InboundMessage::CompareBytesToGrant cbg;
                    return cbg(*lhs->mesgsToGrant.begin(),
                        *rhs->mesgsToGrant.begin());
                }
            };

            /**
             * This class is a container to keep a copy of scheduled state of
             * the receiver scheduler. This is used to track state changes after
             * each new event arrives at the ReceiveScheduler.
             */
            class SchedState {
              PUBLIC:
                // Total number of senders we allow to be scheduled and granted
                // at the same time (ie. overcommittment level).
                int numToGrant;

                // Index of the highest priority sender in the list. This could
                // be non zero which means no sender will be located at indexes
                // 0 to headIdx-1 of the senders list.
                int headIdx;

                // Total number of senders in the list.
                int numSenders;

                // SenderState for which we have handled one of the events
                SenderState* s;

                // Index of s in the senders list
                int sInd;

              PUBLIC:
                void setVar(uint16_t numToGrant, uint32_t headIdx,
                    uint32_t numSenders, SenderState* s, int sInd)
                {
                    this->numToGrant = numToGrant;
                    this->headIdx = headIdx;
                    this->numSenders = numSenders;
                    this->s = s;
                    this->sInd = sInd;
                }

                friend std::ostream& operator<< (std::ostream& os,
                        const SchedState& ss) {
                    os << "Sender: " << ss.s->getSenderAddr().str() <<
                    ", senderInd: " << ss.sInd << ", headInd: " << ss.headIdx <<
                    ", numToGrant: " << ss.numToGrant << ", numSenders: " <<
                    ss.numSenders;
                    return os;
                }
            }; //end SchedState

            std::tuple<int, int, int> insPoint(SenderState* s);
            void insert(SenderState* s);
            int remove(SenderState* s);
            SenderState* removeAt(uint32_t rmInd);
            uint32_t numActiveSenders();

            //void handleGrantRequest(SenderState* s, int sInd, int headInd);
            uint16_t getPrioForMesg(SchedState& cur);
            void handleBwUtilTimerEvent(cMessage* timer);
            void handlePktArrivalEvent(SchedState& old, SchedState& cur);
            void handleGrantSentEvent(SchedState& old, SchedState& cur);
            void handleMesgRecvCompletionEvent(
                const std::pair<bool, int>& msgCompHandle,
                SchedState& old, SchedState& cur);
            void handleGrantTimerEvent(SenderState* s);

          PROTECTED:
            // Back pointer to the transport module.
            HomaTransport* transport;

            // Back pointer to the ReceiveScheduler managing this container
            ReceiveScheduler* rxScheduler;

            // Sorted list of all scheduled senders.
            std::vector<SenderState*> senders;

            // Total number available priorities for the scheduled packets.
            uint16_t schedPrios;

            // Total number of senders we allow to be scheduled and granted at
            // the same time (ie. overcommittment level).
            uint16_t numToGrant;

            // Index of the highest priority sender in the list. This could be
            // non zero which means no sender will be located at indexes 0 to
            // headIdx-1 of the senders list.
            uint32_t headIdx;

            // Total number of senders in the list.
            uint32_t numSenders;

            //Collection of user provided config parameters for the transport.
            HomaConfigDepot* homaConfig;

            friend class HomaTransport::ReceiveScheduler;
        }; //end SchedSenders

      PROTECTED:

        // Back pointer to the transport instance that owns this scheduler.
        HomaTransport* transport;

        // Collection of user specified config parameters.
        HomaConfigDepot *homaConfig;

        // Object for computing the average received rate of unscheduled bytes.
        UnschedRateComputer* unschRateComp;

        // Hash container for accessing each sender's state collection using
        // that sender's IP.
        std::unordered_map<uint32_t, SenderState*> ipSendersMap;

        // Hash container for accessing each sender's state collection using
        // from the grant timer object of the sender.
        std::unordered_map<cMessage*, SenderState*> grantTimersMap;

        // Collection of all senders that have at least one message that is not
        // fully granted.
        SchedSenders* schedSenders;

        // Timer for detecting if the receiver's scheduled bandwidth is being
        // wasted. ie. the senders are delaying sending the grants back.
        cMessage* schedBwUtilTimer;

        // The lenght of time interval during which if we don't receive a
        // packet, receiver inbound bw is considered wasted.
        simtime_t bwCheckInterval;

        //*******************************************************//
        //*****Below variables are for statistic collection******//
        //*******************************************************//

        // These variables track outstanding bytes (ie. not yet arrived bytes)
        // that the receiver knows they must arrive in the future.
        std::vector<uint32_t> inflightUnschedPerPrio;
        std::vector<uint32_t> inflightSchedPerPrio;
        uint64_t inflightSchedBytes;
        uint64_t inflightUnschedBytes;

        // The vector below is of size allPrio and each element of the vector is
        // a monotoically increasing number that tracks total number of bytes
        // received on that priority through out the simulation.  Used for
        // statistics collection.
        std::vector<uint64_t> bytesRecvdPerPrio;

        // The vector below is of size allPrio and each element of the vector is
        // a monotoically increasing number that tracks total number of bytes
        // granted on that priority through out the simulation.  Used for
        // statistics collection.
        std::vector<uint64_t> scheduledBytesPerPrio;

        // The vector below is of size allPrio and each element of the vector is
        // a monotoically increasing number that tracks total number of unsched
        // bytes that are expected to be received on that priority through out
        // the simulation. Used for statistics collection.
        std::vector<uint64_t> unschedToRecvPerPrio;

        // A monotonically increasing number that tracks total number of bytes
        // received throughout the simulation. Used for statistics collection.
        uint64_t allBytesRecvd;

        // A monotonically increasing number that tracks total number of unsched
        // bytes to be received throughout the simulation. Used for statistics
        // collection.
        uint64_t unschedBytesToRecv;

        // Tracks the begining of time periods during which outstanding bytes is
        // nonzero. Such periods are defined as active periods.
        simtime_t activePeriodStart;

        // Tracks the bytes received during each active period.
        uint64_t rcvdBytesPerActivePeriod;

        // Tracks the begining and end of time periods during which total number
        // of senders with scheduled messages is larger than redundancy factor.
        // This period is called an over subscription period.
        simtime_t oversubPeriodStart, oversubPeriodStop;

        // Only true when an over subscription period has started not yet ended.
        bool inOversubPeriod;

        // Tracks the bytes received during each over subscription period.
        uint64_t rcvdBytesPerOversubPeriod;

        // In Homa, number of scheduled senders that receiver is granting
        // at any point of time changes depending on the number of senders and
        // numToGrant and the receiver's bandwidth that is being wasted. This
        // varible tracks the current number sched senders that  receiver is
        // actively granting. The value of this variable is obtained through
        // calling SchedSenders::numActiveSenders() method.
        uint16_t numActiveScheds;

        // Tracks the last simulation time at which numActiveScheds has changed.
        simtime_t schedChangeTime;

        // Tracks the time at which we received the last data packet
        simtime_t lastRecvTime;

      PROTECTED:
        void initialize(HomaConfigDepot* homaConfig,
            PriorityResolver* prioResolver);
        void processReceivedPkt(HomaPkt* rxPkt);
        void processGrantTimers(cMessage* grantTimer);

        inline uint64_t getInflightBytes()
        {
            return inflightUnschedBytes + inflightSchedBytes;
        }

        inline const std::vector<uint32_t>& getInflightUnschedPerPrio()
        {
            return inflightUnschedPerPrio;
        }

        inline const std::vector<uint32_t>& getInflightSchedPerPrio()
        {
            return inflightSchedPerPrio;
        }

        void addArrivedBytes(PktType pktType, uint16_t prio,
            uint32_t dataBytes);
        void addPendingGrantedBytes(uint16_t prio, uint32_t grantedBytes);
        void addPendingUnschedBytes(PktType pktType, uint16_t prio,
            uint32_t bytesToArrive);
        void pendingBytesArrived(PktType pktType, uint16_t prio,
            uint32_t dataBytesInPkt);
        void tryRecordActiveMesgStats(simtime_t timeNow);
        friend class HomaTransport;
        friend class SchedSenders;
        friend class InboundMessage;
    }; //end ReceiveScheduler

    /**
     * Keeps record of the reported rtt samples for each sender and provides max
     * value of RTT between all senders, ie. RTT on the longest path in the
     * network.
     */
    class TrackRTTs
    {
      PUBLIC:
        TrackRTTs(HomaTransport* transport);
        ~TrackRTTs(){}
        void updateRTTSample(uint32_t senderIP, simtime_t rttVal);

      PUBLIC:
        // For each sender, keeps track of the max of reported observed RTT for
        // that sender.
        std::unordered_map<uint32_t, simtime_t> sendersRTT;

        // This is the largest of minimum RTT value observed from senders so
        // far. It will track
        std::pair<uint32_t, simtime_t> maxRTT;

        // The transport module that owns the instanace of this class.
        HomaTransport* transport;
    };

  PUBLIC:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
    virtual void finish();
    const inet::L3Address& getLocalAddr() {return localAddr;}
    void handleRecvdPkt(cPacket* ptk);
    void processStart();
    void testAndEmitStabilitySignal();
    void registerTemplatedStats();

    /**
     * A self message essentially models a timer for this transport and can have
     * one of the below types.
     */
    enum SelfMsgKind
    {
        START = 1,   // When the transport is in initialization phase.
        GRANT = 2,   // For the grant timer when it's sending grants.
        SEND  = 3,   // For the send timer, under normal transmit state.
        EMITTER = 4, // When emitSignalTimer is ready to be scheduled.
        BW_CHECK = 5,// For schedBwUtilTimer, when it's active.
        STOP  = 6    // When trasnport shutting down and in the cleaning phase.
    };

  PUBLIC:

    // Handles the transmission of outbound messages based on the logic of
    // HomaProtocol.
    SendController sxController;

    // Manages the reception of all inbound messages.
    ReceiveScheduler rxScheduler;

    // Keeps records of the smallest rtt observed from different senders and the
    // max of those observations as the RTT of the network.
    TrackRTTs trackRTTs;

    // Templated signals for tracking priority usage statistics.
    std::vector<simsignal_t> priorityStatsSignals;

  PROTECTED:

    // UDP socket through which this transport send and receive packets.
    UDPSocket socket;

    // IpAddress of sender host (local host). This parameter is lazily
    // intialized first time an outbound message is arrvied from application or
    // a packet has arrived from outside world.
    inet::L3Address localAddr;

    // The object that keeps the configuration parameters for this transport
    HomaConfigDepot *homaConfig;

    // Determine priority of packets that are to be sent
    PriorityResolver *prioResolver;

    // Keeps track of the message size distribution that this transport is
    // seeing.
    WorkloadEstimator *distEstimator;

    // Timer object for send side packet pacing. At every packet transmission at
    // sender this will be used to schedule next send after the current send is
    // completed.
    cMessage* sendTimer;

    // This timer is used to priodically emit signals that are received at the
    // global simulation level by the GlobalSignalListener.
    cMessage* emitSignalTimer;

    // Tracks the next schedule time for emitSignalTimer and initialized to zero
    simtime_t nextEmitSignalTime;

    // Tracks the total outstanding grant bytes which will be used for stats
    // collection and recording.
    int outstandingGrantBytes;

    friend class ReceiveScheduler;
    friend class SendController;

  PUBLIC:
    /**
     * C++ declration of signals defined in .ned file.
     */
    // Signal for number of incomplete TX msgs received from the application.
    static simsignal_t msgsLeftToSendSignal;

    // This signal is priodically emitted and carries number of remaining
    // messages yet to be transmitted. This signal is intended to be receveid
    // by GlobalSignalListener which record stability metrics at the top
    // simulation level.
    static simsignal_t stabilitySignal;

    // Signal for total number of msg bytes yet to be sent for all msgs.
    static simsignal_t bytesLeftToSendSignal;

    // Signal for total sched. bytes yet to receive grants from their receivers
    static simsignal_t bytesNeedGrantSignal;

    // Signal for total number of in flight grant bytes.
    static simsignal_t outstandingGrantBytesSignal;

    // Signal for total number of in flight bytes including both grants and
    // unscheduled packets.
    static simsignal_t totalOutstandingBytesSignal;

    // Signal for recording times during which receiver outstanding bytes is non
    // zero.
    static simsignal_t rxActiveTimeSignal;

    // Signal for recording bytes received during each active time. Along with
    // acitveTimeSignal, will help us to find receiver wasted bw as result of
    // pkts delayed because of queuing or senders delaying scheduled pkts.
    static simsignal_t rxActiveBytesSignal;

    // Signal for recording time during which the receiver is oversubscribed;
    // ie. number of senders is larger than number of numSendersToKeepGranted.
    static simsignal_t oversubTimeSignal;

    // Signal for recording bytes received during each oversubscribed period.
    // Along with oversubTimeSignal, will help us to find receiver wasted bw as
    // result of not scheduling all the possible senders.
    static simsignal_t oversubBytesSignal;

    // Singals for recording time durations that receiver's inbound bw is wasted
    // and that wasted bw may have been avoided if the receiver granted more
    // senders than the number it currently granted. The higher and lower
    // prefixes are for "Higher Overestimate" and "Lower Overestimate" of this
    // metric.
    static simsignal_t higherRxSelfWasteSignal;
    static simsignal_t lowerRxSelfWasteSignal;

    // Signal for recording time periods during which sender has bytes awaiting
    // tranmission.
    static simsignal_t sxActiveTimeSignal;

    // Signal for recording total bytes transmitted during each active time.
    // Along with active time, this helps us figure wasted bw at the sender
    // because of not receiving timely grants from receiver.
    static simsignal_t sxActiveBytesSignal;

    // Signal for recording sender's delay in transmitting sched. packets. This
    // delay may translate to bubbles in the receiver's link.
    static simsignal_t sxSchedPktDelaySignal;

    // Signal for recording sender delay in transmitting unsched. packets that
    // might translate to bubbles in receiver's link.
    static simsignal_t sxUnschedPktDelaySignal;

    // When the number of active scheduled sender (ie. number of sender actively
    // receiving grants from the receiver) change, this signal is emitted and
    // carries the last value of active scheduled senders and time duration that
    // value was maintained.
    static simsignal_t activeSchedsSignal;
};//End HomaTransport

/**
 * To create cumulative-time-percent statistics of active scheduled
 * senders, we send activeSchedsSignal to GlobalSignalListener which
 * dumps that stats. Thise signal carries a pointer to object of type
 * ActiveScheds class and this object contais the last value of
 * numActiveSenders and time duration that the scheduler was keeping
 * that many active scheduled senders.
 */
class ActiveScheds : public cObject, noncopyable
{
  PUBLIC:
    ActiveScheds(){}

  PUBLIC:
    uint32_t numActiveSenders;
    simtime_t duration;
};//end ActiveScheds

#endif
