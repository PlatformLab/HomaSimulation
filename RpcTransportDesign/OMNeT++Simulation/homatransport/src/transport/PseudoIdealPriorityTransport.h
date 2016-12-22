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

#ifndef __HOMATRANSPORT_PSEUDOIDEALPRIORITYTRANSPORT_H_
#define __HOMATRANSPORT_PSEUDOIDEALPRIORITYTRANSPORT_H_

#include <omnetpp.h>
#include <unordered_map>
#include <list>
#include "common/Minimal.h"
#include "inet/transportlayer/contract/udp/UDPSocket.h"
#include "application/AppMessage_m.h"
#include "transport/HomaPkt.h"

/**
 * An near optimal priority based transport scheme for minimizing the average
 * completion time of messages. For every packet of a message, it sets the
 * message size field and a priority field equal to the remaining bytes of the
 * message not yet send. This transport will only be near optimal if the network
 * has priority queues and only for many-senders/single-receiver scenario. The
 * scheduling mechanism for the priority queues in the network would always
 * choose the lowest priority packet that belongs to shortes message among all
 * packets in the queue.
 */

class PseudoIdealPriorityTransport : public cSimpleModule
{
  PUBLIC:
    // Signal definitions for statistics gathering
    static simsignal_t msgsLeftToSendSignal;
    static simsignal_t bytesLeftToSendSignal;

  PUBLIC:
    PseudoIdealPriorityTransport();
    ~PseudoIdealPriorityTransport();

  PROTECTED:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
    virtual void processStart();
    virtual void processStop();
    virtual void processMsgFromApp(AppMessage* sendMsg);
    virtual void processRcvdPkt(HomaPkt* rxPkt);
    virtual void finish();

    /**
     * A self message essentially models a timer object for this transport and
     * can have one of the following types.
     */
    enum SelfMsgKind
    {
        START = 1,  // Timer type when the transport is in initialization phase.
        STOP  = 2   // Timer type when the transport is in cleaning phase.
    };

    class InboundMsg
    {
      PUBLIC:
        explicit InboundMsg();
        explicit InboundMsg(HomaPkt* rxPkt);
        ~InboundMsg();
        bool appendPktData(HomaPkt* rxPkt);

      PUBLIC:
        int numBytesToRecv;
        uint32_t msgByteLen;
        uint32_t totalBytesOnWire;
        inet::L3Address srcAddr;
        inet::L3Address destAddr;
        uint64_t msgIdAtSender;
        simtime_t msgCreationTime;
    };

  PROTECTED:

    // UDP socket through which this transport send and receive packets.
    inet::UDPSocket socket;

    // Timer object for this transport. Will be used for implementing timely
    // scheduled
    cMessage* selfMsg;

    // udp ports through which this transport send and receive packets
    int localPort;
    int destPort;

    // variables and states kept for administering outbound messages
    uint64_t msgId; // unique monotonically increasing id for
                    // each messages to send
    uint32_t maxDataBytesInPkt;

    // State and variables kept for managing inbound messages
    // Defines a map to keep a all partially received inbound messages. The key
    // is the msgId at the sender and value is a list of pointer to rx
    // messages currently under transmission from different senders that
    // happened to have same id at the sender side.
    typedef std::unordered_map<uint64_t, std::list<InboundMsg*>>
            IncompleteRxMsgsMap;
    IncompleteRxMsgsMap incompleteRxMsgsMap;
};

#endif
