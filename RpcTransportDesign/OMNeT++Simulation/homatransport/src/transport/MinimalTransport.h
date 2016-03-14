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

#ifndef __HOMATRANSPORT_MINIMALTRANSPORT_H_
#define __HOMATRANSPORT_MINIMALTRANSPORT_H_

#include <omnetpp.h>
#include "common/Minimal.h"
#include "transport/OracleGreedySRPTScheduler.h"
#include "inet/transportlayer/contract/udp/UDPSocket.h"
#include "application/AppMessage_m.h"
#include "transport/HomaPkt.h"

/**
 * A very light end host transport layer that every packet time, transmits a new
 * packet from a message that has been scheduled for transmission by the Oracle
 * Scheduler.
 */
class MinimalTransport : public cSimpleModule
{
  PUBLIC:
    MinimalTransport();
    ~MinimalTransport();
    enum SelfMesgKind {
        START  = 1,
        TRANSMITTING
    };
    typedef OracleGreedySRPTScheduler::MesgChunk MesgChunk;

  PROTECTED:
    virtual void initialize();
    virtual void handleMessage(cMessage *mesg);
    virtual void finish();
    void processStart();
    void processTransmitPkt();
    void processMsgFromApp(AppMessage* mesg);
    void processRcvdPkt(HomaPkt* mesg);
    void refreshSchedule();
    friend class OracleGreedySRPTScheduler;

  PROTECTED:
    // Signal definitions for statistics gathering
    static simsignal_t msgsLeftToSendSignal;
    static simsignal_t bytesLeftToSendSignal;

    // Total remaining bytes to transmit
    uint64_t bytesLeftToSend;

    // Total number of messages not transmitted yet
    uint64_t msgsLeftToSend;

    // UDP socket through which this transport send and receive packets.
    inet::UDPSocket socket;

    // Timer object for this transport. Will be used for implementing timely
    // scheduled
    cMessage* selfMesg;

    // udp ports through which this transport send and receive packets
    int localPort;
    int destPort;

    // Network iface link speed in Gb/s. Given as simulation config parameter
    int nicLinkSpeed; 

    // xml object that contains extra config parameter values
    cXMLElement* xmlConfig;

    // IP address of the network interface
    inet::L3Address localAddr;

    // Pointer to the oracle scheduler that schedules message transmission for
    // this end host transport.
    OracleGreedySRPTScheduler* oracleScheduler;

    // max possible data bytes to be transmitted in each packet
    uint16_t maxDataBytesPerPkt;
};

#endif //end __HOMATRANSPORT_MINIMALTRANSPORT_H_
