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

  protected:
    enum SelfMsgKinds { START = 1, SEND, STOP };
    enum InterArrivalDist {
        EXPONENTIAL = 1,
        GAUSSIAN,
        FACEBOOK_KEY_VALU,
        CONSTANT
    };

    // generates samples from the a given random distribution 
    MsgSizeDistributions *msgSizeGenerator;

    // parameters
    std::vector<inet::L3Address> destAddresses;
    simtime_t startTime;
    simtime_t stopTime;
    InterArrivalDist interArrivalDist;
    double avgInterArrivalTime;
    int maxDataBytesPerPkt;
    cXMLElement* xmlConfig;

    // states
    cMessage* selfMsg;
    inet::L3Address srcAddress;

    // statistics
    int numSent;
    int numReceived;

    static simsignal_t sentMsgSignal;
    static simsignal_t rcvdMsgSignal;
    static simsignal_t msgE2EDelaySignal;

    static simsignal_t msg1PktE2EDelaySignal;
    static simsignal_t msg3PktsE2EDelaySignal;
    static simsignal_t msg6PktsE2EDelaySignal;
    static simsignal_t msg13PktsE2EDelaySignal;
    static simsignal_t msg33PktsE2EDelaySignal;
    static simsignal_t msg133PktsE2EDelaySignal;
    static simsignal_t msg1333PktsE2EDelaySignal;
    static simsignal_t msgHugeE2EDelaySignal;

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
    double nextSendTime();
    void parseAndProcessXMLConfig();
};

#endif //__HOMATRANSPORT_WORKLOADSYNTHESIZER_H_
