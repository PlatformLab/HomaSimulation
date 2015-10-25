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

/**
 * An near optimal priority based transport scheme for minimizing the average
 * completion time of messages. For every packet of a message, it sets the
 * message size field and a priority field equal to the remaining bytes of the
 * message not yet send. This transport will only be near optimal if the
 * network has priority queues. The scheduling mechanism for the priority
 * queues in the network would always choose the lowest priority packet that
 * belongs to shortes message among all packets in the queue. 
 */

class PseudoIdealPriorityTransport : public cSimpleModule
{
  public:
    PseudoIdealPriorityTransport();
    ~PseudoIdealPriorityTransport();
    
  protected:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);

    /**
     * A self message essentially models a timer object for this transport and
     * can have one of the following types.
     */
    enum SelfMsgKind
    {
        START = 1,  // Timer type when the transport is in initialization phase.
        GRANT = 2,  // Timer type in normal working state of transport.
        STOP  = 3   // Timer type when the transport is in cleaning phase.
    };

  protected:

    // Signal definitions for statistics gathering
    simsignal_t msgsLeftToSendSignal;
    simsignal_t bytesLeftToSend;

    // udp ports through which this transport send and receive packets
    int localPort;
    int destPort;

};

#endif
