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
#include "application/MsgSizeDistributions.h"
#include "application/AppMessage_m.h"
#include "inet/networklayer/common/L3Address.h"


/**
 * Mocks message generating behaviour of an application. Given a messge size
 * distribution and a interarrival time distribution, this module generates
 * message sizes of that distribution and sends them in interarrival times 
 * sampled from the interarrival distribution. 
 */
class WorkloadSynthesizer : public cSimpleModule
{

  public:
    WorkloadSynthesizer();
    ~WorkloadSynthesizer();
    static const uint32_t MAX_ETHERNET_PAYLOAD_BYTES = 1500;
    static const uint32_t IP_HEADER_SIZE = 20;
    static const uint32_t UDP_HEADER_SIZE = 8;
    static const uint32_t ETHERNET_PREAMBLE_SIZE = 8;
    static const uint32_t ETHERNET_HDR_SIZE = 14; 
    static const uint32_t ETHERNET_CRC_SIZE = 4;
    static const uint32_t MIN_ETHERNET_FRAME_SIZE = 64; 

  protected:
    enum SelfMsgKinds { START = 1, SEND, STOP };

    // generates samples from the a given random distribution 
    MsgSizeDistributions *msgSizeGenerator;

    // parameters
    std::vector<inet::L3Address> destAddresses;
    simtime_t startTime;
    simtime_t stopTime;
    bool isSender;
    int maxDataBytesPerPkt;
    cXMLElement* xmlConfig;
    uint32_t nicLinkSpeed; // in Gb/s
    uint32_t fabricLinkSpeed;
    double fabricLinkDelay;
    double edgeLinkDelay;
    double nicThinkTime;

    // states
    cMessage* selfMsg;
    inet::L3Address srcAddress;
    int sendMsgSize; // In bytes

    // statistics
    int numSent;
    int numReceived;

    static simsignal_t sentMsgSignal;
    static simsignal_t rcvdMsgSignal;

    // Signal for end to end to delay every received messages
    static simsignal_t msgE2EDelaySignal; 

    // Signals for end to end delay of message ranges. In the signal name 
    // msgXPktE2EDelaySignal, X stands for messages sizes that are smaller or
    // equal to X and larger than previously defined signal.
    static simsignal_t msg1PktE2EDelaySignal;
    static simsignal_t msg3PktsE2EDelaySignal;
    static simsignal_t msg6PktsE2EDelaySignal;
    static simsignal_t msg13PktsE2EDelaySignal;
    static simsignal_t msg33PktsE2EDelaySignal;
    static simsignal_t msg133PktsE2EDelaySignal;
    static simsignal_t msg1333PktsE2EDelaySignal;
    static simsignal_t msgHugeE2EDelaySignal;

    // Signals for end to end stretch factor of message different ranges.
    // Stretch is defined as the overhead factor end to end completion time of a
    // mesage comparing to ideal condition.  In the signal name
    // msgXPktE2EStretchSignal, X stands for messages sizes that are smaller
    // or equal to X and larger than previously defined signal.
    static simsignal_t msg1PktE2EStretchSignal;
    static simsignal_t msg3PktsE2EStretchSignal;
    static simsignal_t msg6PktsE2EStretchSignal;
    static simsignal_t msg13PktsE2EStretchSignal;
    static simsignal_t msg33PktsE2EStretchSignal;
    static simsignal_t msg133PktsE2EStretchSignal;
    static simsignal_t msg1333PktsE2EStretchSignal;
    static simsignal_t msgHugeE2EStretchSignal;

    // Signals for network queuing delay overhead for different msg size ranges.
    // In the signal name msgXPktQueuingDelaySignal, X stands for messages sizes
    // that are smaller or equal to X and larger than previously defined signal.
    static simsignal_t msg1PktQueuingDelaySignal;
    static simsignal_t msg3PktsQueuingDelaySignal;
    static simsignal_t msg6PktsQueuingDelaySignal;
    static simsignal_t msg13PktsQueuingDelaySignal;
    static simsignal_t msg33PktsQueuingDelaySignal;
    static simsignal_t msg133PktsQueuingDelaySignal;
    static simsignal_t msg1333PktsQueuingDelaySignal;
    static simsignal_t msgHugeQueuingDelaySignal;



  protected:
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
    double idealMsgEndToEndDelay(AppMessage* rcvdMsg);
    
};

#endif //__HOMATRANSPORT_WORKLOADSYNTHESIZER_H_
