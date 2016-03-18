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

#ifndef __HOMATRANSPORT_WORKLOADSYNTHESIZER_H_
#define __HOMATRANSPORT_WORKLOADSYNTHESIZER_H_

#include <omnetpp.h>
#include <vector>
#include <string>
#include <sstream>
#include "inet/networklayer/common/L3Address.h"
#include "common/Minimal.h"
#include "application/MsgSizeDistributions.h"
#include "application/AppMessage_m.h"
#include "transport/HomaConfigDepot.h"

/**
 * Mocks message generating behaviour of an application. Given a messge size
 * distribution and a interarrival time distribution, this module generates
 * message sizes of the size distribution and sends them in interarrival times
 * sampled from the interarrival distribution.
 */
class WorkloadSynthesizer : public cSimpleModule
{
  PUBLIC:
    WorkloadSynthesizer();
    ~WorkloadSynthesizer();

  PROTECTED:
    enum SelfMsgKinds { START = 1, SEND, STOP };

    // generates samples from given random distributions for message size and
    // inter arrival times.
    MsgSizeDistributions *msgSizeGenerator;

    // parameters from NED files
    std::vector<inet::L3Address> destAddresses;
    bool isSender;
    simtime_t startTime;
    simtime_t stopTime;
    cXMLElement* xmlConfig;
    uint32_t nicLinkSpeed; // in Gb/s

    // Timer object for event generation
    cMessage* selfMsg;

    // Ip address of the local host
    inet::L3Address srcAddress;

    // holds the size (in bytes) of the next message to be transmitted
    int txMsgSize;

    // statistics
    int numSent;
    int numReceived;

  PROTECTED:
    virtual void initialize();
    virtual void handleMessage(cMessage* msg);
    virtual void finish();

    inet::L3Address chooseDestAddr();
    void processStart();
    void processSend();
    void processStop();
    void processRcvdMsg(cPacket* msg);
    void sendMsg();
    void setupNextSend();
    void parseAndProcessXMLConfig();
};

#endif //__HOMATRANSPORT_WORKLOADSYNTHESIZER_H_
