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

#include <unordered_set>
#include <iterator>
#include <iostream>
#include "RpcHandler.h"
#include "transport/HomaPkt.h"

Define_Module(RpcHandler);
simsignal_t RpcHandler::sentMsgSignal = registerSignal("sentMsg");
simsignal_t RpcHandler::rcvdMsgSignal = registerSignal("rcvdMsg");
simsignal_t RpcHandler::msgE2EDelaySignal = registerSignal("msgE2EDelay");

void
RpcHandler::initialize()
{
    // read in module parameters
    nicLinkSpeed = par("nicLinkSpeed").longValue();
    fabricLinkSpeed = par("fabricLinkSpeed").longValue();
    edgeLinkDelay = 1e-6 * par("edgeLinkDelay").doubleValue();
    fabricLinkDelay = 1e-6 * par("fabricLinkDelay").doubleValue();
    hostSwTurnAroundTime = 1e-6 * par("hostSwTurnAroundTime").doubleValue();
    hostNicSxThinkTime = 1e-6 * par("hostNicSxThinkTime").doubleValue();
    switchFixDelay = 1e-6 * par("switchFixDelay").doubleValue();
    isFabricCutThrough = par("isFabricCutThrough").boolValue();
    isSingleSpeedFabric = par("isSingleSpeedFabric").boolValue();

    // Setup templated statistics ans signals
    const char* msgSizeRanges = par("msgSizeRanges").stringValue();
    registerTemplatedStats(msgSizeRanges);
}

void
RpcHandler::registerTemplatedStats(const char* msgSizeRanges)
{
    std::stringstream sstream(msgSizeRanges);
    std::istream_iterator<uint64_t> begin(sstream);
    std::istream_iterator<uint64_t> end;
    std::vector<uint64_t> sizeRangeVec(begin, end);
    for (size_t i = 0; i <= sizeRangeVec.size(); i++) {
        std::string sizeUpperBound;
        if (i < sizeRangeVec.size()) {
            sizeUpperBound = std::to_string(sizeRangeVec[i]);
            msgSizeRangeUpperBounds.push_back(sizeRangeVec[i]);
        } else {
            sizeUpperBound = "Huge";
        }

        char latencySignalName[50];
        sprintf(latencySignalName, "msg%sE2EDelay", sizeUpperBound.c_str());
        simsignal_t latencySignal = registerSignal(latencySignalName);
        msgE2ELatencySignalVec.push_back(latencySignal);
        char latencyStatsName[50];
        sprintf(latencyStatsName, "msg%sE2EDelay", sizeUpperBound.c_str());
        cProperty *statisticTemplate =
                getProperties()->get("statisticTemplate", "msgRangesE2EDelay");
        ev.addResultRecorders(this, latencySignal, latencyStatsName,
                statisticTemplate);

        char queueDelaySignalName[50];
        sprintf(queueDelaySignalName, "msg%sQueuingDelay",
                sizeUpperBound.c_str());
        simsignal_t queueDelaySignal = registerSignal(queueDelaySignalName);
        msgQueueDelaySignalVec.push_back(queueDelaySignal);
        char queueDelayStatsName[50];
        sprintf(queueDelayStatsName, "msg%sQueuingDelay",
                sizeUpperBound.c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "msgRangesQueuingDelay");
        ev.addResultRecorders(this, queueDelaySignal, queueDelayStatsName,
                statisticTemplate);

        char stretchSignalName[50];
        sprintf(stretchSignalName, "msg%sE2EStretch", sizeUpperBound.c_str());
        simsignal_t stretchSignal = registerSignal(stretchSignalName);
        msgE2EStretchSignalVec.push_back(stretchSignal);
        char stretchStatsName[50];
        sprintf(stretchStatsName, "msg%sE2EStretch", sizeUpperBound.c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "msgRangesE2EStretch");
        ev.addResultRecorders(this, stretchSignal, stretchStatsName,
                statisticTemplate);

        char transportSchedDelaySignalName[50];
        sprintf(transportSchedDelaySignalName, "msg%sTransportSchedDelay",
            sizeUpperBound.c_str());
        simsignal_t transportSchedDelaySignal =
            registerSignal(transportSchedDelaySignalName);
        msgTransprotSchedDelaySignalVec.push_back(transportSchedDelaySignal);
        char transportSchedDelayStatsName[50];
        sprintf(transportSchedDelayStatsName, "msg%sTransportSchedDelay",
            sizeUpperBound.c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "msgRangesTransportSchedDelay");
        ev.addResultRecorders(this, transportSchedDelaySignal,
            transportSchedDelayStatsName, statisticTemplate);

        char transportSchedPreemptionLagSignalName[50];
        sprintf(transportSchedPreemptionLagSignalName,
            "msg%sTransportSchedPreemptionLag", sizeUpperBound.c_str());
        simsignal_t transportSchedPreemptionLagSignal =
            registerSignal(transportSchedPreemptionLagSignalName);
        msgTransprotSchedPreemptionLagSignalVec.push_back(
            transportSchedPreemptionLagSignal);
        char transportSchedPreemptionLagStatsName[50];
        sprintf(transportSchedPreemptionLagStatsName,
            "msg%sTransportSchedPreemptionLag", sizeUpperBound.c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "msgRangesTransportSchedPreemptionLag");
        ev.addResultRecorders(this, transportSchedPreemptionLagSignal,
            transportSchedPreemptionLagStatsName, statisticTemplate);

        char msgBytesOnWireSignalName[50];
        sprintf(msgBytesOnWireSignalName,
            "msg%sBytesOnWire", sizeUpperBound.c_str());
        simsignal_t msgBytesOnWireSignal =
            registerSignal(msgBytesOnWireSignalName);
        msgBytesOnWireSignalVec.push_back(
            msgBytesOnWireSignal);
        char msgBytesOnWireStatsName[50];
        sprintf(msgBytesOnWireStatsName,
            "msg%sBytesOnWire", sizeUpperBound.c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "msgRangesBytesOnWire");
        ev.addResultRecorders(this, msgBytesOnWireSignal,
            msgBytesOnWireStatsName, statisticTemplate);
    }
}

void
RpcHandler::handleMessage(cMessage *msg)
{
    if (msg->arrivedOn("appIn")) {
        processRcvdAppMsg(check_and_cast<AppMessage*>(msg));
    } else if (msg->arrivedOn("transportIn")) {
        processRcvdRpc(check_and_cast<Rpc*>(msg));
    } else {
        throw cRuntimeError("Message arrived on unrecognized gate.");
    }
}


void
RpcHandler::processRcvdAppMsg(AppMessage* msgFromApp)
{
    char msgName[100];
    sprintf(msgName, "RpcHandlerMsg-%d", numSent);
    Rpc *rpcMessage = new Rpc(msgName);
    rpcMessage->setByteLength(msgFromApp->getByteLength());
    rpcMessage->setDestAddr(msgFromApp->getDestAddr());
    rpcMessage->setSrcAddr(msgFromApp->getSrcAddr());
    rpcMessage->setMsgCreationTime(simTime());
    rpcMessage->setTransportSchedDelay(simTime());
    rpcMessage->setTransportSchedPreemptionLag(simTime());
    emit(sentMsgSignal, rpcMessage);
    send(rpcMessage, "transportOut");
    numSent++;
    delete msgFromApp;
}

void
RpcHandler::processRcvdRpc(Rpc* rpcFromTransport)
{
    // Construct an app message and send to the application
    AppMessage* rcvdMsg = new AppMessage();
    rcvdMsg->setByteLength(rpcFromTransport->getByteLength());
    rcvdMsg->setSrcAddr(rpcFromTransport->getSrcAddr());
    rcvdMsg->setDestAddr(rpcFromTransport->getDestAddr());
    send(rcvdMsg, "appOut", 0);

    // Record relevant statistics for this message
    emit(rcvdMsgSignal, rpcFromTransport);
    simtime_t completionTime =
        simTime() - rpcFromTransport->getMsgCreationTime();
    emit(msgE2EDelaySignal, completionTime);
    uint64_t msgByteLen = (uint64_t)(rpcFromTransport->getByteLength());
    EV_INFO << "Received a message of length " << msgByteLen
            << "Bytes" << endl;

    double idealDelay = idealMsgEndToEndDelay(rpcFromTransport);
    double queuingDelay =  completionTime.dbl() - idealDelay;
    double stretchFactor =
            (idealDelay == 0.0 ? 1.0 : completionTime.dbl()/idealDelay);

    if (msgByteLen > msgSizeRangeUpperBounds.back())  {
        // if messages size doesn't fit in any bins, then it should be assigned
        // to the last overflow (HugeSize) bin
        emit(msgE2ELatencySignalVec.back(), completionTime);
        emit(msgE2EStretchSignalVec.back(), stretchFactor);
        emit(msgQueueDelaySignalVec.back(), queuingDelay);
        emit(msgTransprotSchedDelaySignalVec.back(),
            rpcFromTransport->getTransportSchedDelay());
        emit(msgTransprotSchedPreemptionLagSignalVec.back(),
            rpcFromTransport->getTransportSchedPreemptionLag());
        emit(msgBytesOnWireSignalVec.back(),
            rpcFromTransport->getMsgBytesOnWire());

    } else {
        size_t mid, high, low;
        high = msgSizeRangeUpperBounds.size() - 1;
        low = 0;
        while(low < high) {
            mid = (high + low) / 2;
            if (msgByteLen <= msgSizeRangeUpperBounds[mid]) {
                high = mid;
            } else {
                low = mid + 1;
            }
        }
        emit(msgE2ELatencySignalVec[high], completionTime);
        emit(msgE2EStretchSignalVec[high], stretchFactor);
        emit(msgQueueDelaySignalVec[high], queuingDelay);
        emit(msgTransprotSchedDelaySignalVec[high],
            rpcFromTransport->getTransportSchedDelay());
        emit(msgTransprotSchedPreemptionLagSignalVec[high],
            rpcFromTransport->getTransportSchedPreemptionLag());
        emit(msgBytesOnWireSignalVec[high],
            rpcFromTransport->getMsgBytesOnWire());
    }
    delete rpcFromTransport;
    numRecvd++;
}

/**
 * The proper working of this part of the code depends on the
 * correct ip assignments based on the config.xml file.
 */
double
RpcHandler::idealMsgEndToEndDelay(Rpc* rcvdMsg)
{
    int totalBytesTranmitted = 0;
    inet::L3Address srcAddr = rcvdMsg->getSrcAddr();
    ASSERT(srcAddr.getType() == inet::L3Address::AddressType::IPv4);
    inet::L3Address destAddr = rcvdMsg->getDestAddr();
    ASSERT(destAddr.getType() == inet::L3Address::AddressType::IPv4);

    if (destAddr == srcAddr) {
        // no switching delay
        return totalBytesTranmitted;
    }

    // calculate the total transmitted bytes in the the network for this
    // rcvdMsg. These bytes include all headers and ethernet overhead bytes per
    // frame.
    int lastPartialFrameLen = 0;
    int maxDataBytesPerEthFrame =
            MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE - UDP_HEADER_SIZE;
    int numFullEthFrame = rcvdMsg->getByteLength() / maxDataBytesPerEthFrame;
    uint32_t lastPartialFrameData =
            rcvdMsg->getByteLength() % maxDataBytesPerEthFrame;


    totalBytesTranmitted = numFullEthFrame *
            (MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_HDR_SIZE +
            ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP);

    if (lastPartialFrameData == 0) {
        if (numFullEthFrame == 0) {
            totalBytesTranmitted = MIN_ETHERNET_FRAME_SIZE +
                    ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP;
            lastPartialFrameLen = totalBytesTranmitted;
        }

    } else {
        if (lastPartialFrameData < (MIN_ETHERNET_PAYLOAD_BYTES -
                IP_HEADER_SIZE - UDP_HEADER_SIZE)) {
            lastPartialFrameLen = MIN_ETHERNET_FRAME_SIZE +
                    ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP;
        } else {
            lastPartialFrameLen = lastPartialFrameData + IP_HEADER_SIZE
                    + UDP_HEADER_SIZE + ETHERNET_HDR_SIZE + ETHERNET_CRC_SIZE
                    + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP;
        }
        totalBytesTranmitted += lastPartialFrameLen;
    }

    double msgSerializationDelay =
            1e-9 * ((totalBytesTranmitted << 3) * 1.0 / nicLinkSpeed);

    // There's always two hostSwTurnAroundTime and one nicThinkTime involved
    // in ideal latency for the overhead.
    double hostDelayOverheads = 2 * hostSwTurnAroundTime + hostNicSxThinkTime;

    // Depending on if the switch model is store-forward (omnet++ default model)
    // or cutthrough (as we implemented), the switch serialization delay would
    // be different. The code snipet below finds how many switch a packet passes
    // through and adds the correct switch delay to total delay based on the
    // switch model.
    double totalSwitchDelay = 0;

    double edgeSwitchFixDelay = switchFixDelay;
    double fabricSwitchFixDelay = switchFixDelay;
    double edgeSwitchSerialDelay = 0;
    double fabricSwitchSerialDelay = 0;

    if (numFullEthFrame != 0) {
        edgeSwitchSerialDelay +=
                (MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_HDR_SIZE +
                ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP) *
                1e-9 * 8 / nicLinkSpeed;

        fabricSwitchSerialDelay += (MAX_ETHERNET_PAYLOAD_BYTES +
                ETHERNET_HDR_SIZE + ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE +
                INTER_PKT_GAP) * 1e-9 * 8 / fabricLinkSpeed;
    } else {
        edgeSwitchSerialDelay += lastPartialFrameLen * 1e-9 * 8 / nicLinkSpeed;
        fabricSwitchSerialDelay +=
                lastPartialFrameLen * 1e-9 * 8 / fabricLinkSpeed;
    }

    if (destAddr.toIPv4().getDByte(2) == srcAddr.toIPv4().getDByte(2)) {

        // src and dest in the same rack
        totalSwitchDelay = edgeSwitchFixDelay;
        if (!isFabricCutThrough) {
            totalSwitchDelay =+ edgeSwitchSerialDelay;
        }

        // Add 2 edge link delays
        totalSwitchDelay += (2 * edgeLinkDelay);

    } else if (destAddr.toIPv4().getDByte(1) == srcAddr.toIPv4().getDByte(1)) {

        // src and dest in the same pod
        totalSwitchDelay =
                edgeSwitchFixDelay +  fabricSwitchFixDelay + edgeSwitchFixDelay;
        if (!isFabricCutThrough) {
            totalSwitchDelay +=
                    (2*fabricSwitchSerialDelay + edgeSwitchSerialDelay);
        } else if (!isSingleSpeedFabric) {
            // have cutthrough but forwarding a packet coming from low
            // speed port to high speed port. There will inevitably be one
            // serialization at low speed.
            totalSwitchDelay += edgeSwitchSerialDelay;
        }

        // Add 2 edge link delays and two fabric link delays
        totalSwitchDelay += (2 * edgeLinkDelay + 2 * fabricLinkDelay);

    } else {
        totalSwitchDelay = edgeSwitchFixDelay +
                           fabricSwitchFixDelay +
                           fabricSwitchFixDelay +
                           fabricSwitchFixDelay +
                           edgeSwitchFixDelay;
        if (!isFabricCutThrough) {
            totalSwitchDelay += (fabricSwitchSerialDelay +
                    fabricSwitchSerialDelay + fabricSwitchSerialDelay +
                    fabricSwitchSerialDelay + edgeSwitchSerialDelay);
        } else if (!isSingleSpeedFabric) {

            totalSwitchDelay += edgeSwitchFixDelay;
        }

        // Add 2 edge link delays and 4 fabric link delays
        totalSwitchDelay += (2 * edgeLinkDelay + 4 * fabricLinkDelay);
    }
    return msgSerializationDelay + totalSwitchDelay + hostDelayOverheads;
}
