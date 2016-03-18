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

#ifndef __HOMATRANSPORT_RPCHANDLER_H_
#define __HOMATRANSPORT_RPCHANDLER_H_

#include <omnetpp.h>
#include <vector>
#include <string>
#include <sstream>
#include "common/Minimal.h"
#include "inet/networklayer/common/L3Address.h"
#include "application/AppMessage_m.h"
#include "application/Rpc_m.h"

/**
 * This is the intermediate module between the applications and the transport
 * layer. It can manage the requests from multiple applications. It also
 * collects and dumps statistice for messages.
 * TODO: This module is supposed to be RPC handler such that it returns a
 * response for every request from messgaes.
 */
class RpcHandler : public cSimpleModule
{
  PUBLIC:
    RpcHandler(){}
    ~RpcHandler(){}

  PROTECTED:
    // config parameters
    std::vector<inet::L3Address> destAddresses;
    uint32_t nicLinkSpeed; // in Gb/s
    uint32_t fabricLinkSpeed;
    double fabricLinkDelay;
    double edgeLinkDelay;
    double hostSwTurnAroundTime;
    double hostNicSxThinkTime;
    double switchFixDelay;
    bool isFabricCutThrough;
    bool isSingleSpeedFabric;

    int numSent;
    int numRecvd;

    // Signal for end to end to delay every received messages
    static simsignal_t msgE2EDelaySignal;
    static simsignal_t sentMsgSignal;
    static simsignal_t rcvdMsgSignal;

    // keep pairs of upper bound message size range and its corresponding
    // signal id.
    std::vector<uint64_t> msgSizeRangeUpperBounds;
    std::vector<uint64_t> msgBytesOnWireSignalVec;
    std::vector<simsignal_t> msgE2ELatencySignalVec;
    std::vector<simsignal_t> msgE2EStretchSignalVec;
    std::vector<simsignal_t> msgQueueDelaySignalVec;
    std::vector<simsignal_t> msgTransprotSchedDelaySignalVec;
    std::vector<simsignal_t> msgTransprotSchedPreemptionLagSignalVec;

  PROTECTED:
    virtual void initialize();
    virtual void handleMessage(cMessage* msg);
    virtual void finish(){}
    void processRcvdAppMsg(AppMessage* msg);
    void processRcvdRpc(Rpc* msg);
    void registerTemplatedStats(const char* msgSizeRanges);
    double idealMsgEndToEndDelay(Rpc* rcvdMsg);
};
#endif //__HOMATRANSPORT_RPCHANDLER_H_
