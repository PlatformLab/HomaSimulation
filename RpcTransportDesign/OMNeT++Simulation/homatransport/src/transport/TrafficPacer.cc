/*
 * TrafficPacer.cc
 *
 *  Created on: Sep 18, 2015
 *      Author: neverhood
 */

#include <omnetpp.h>
#include "TrafficPacer.h"

TrafficPacer::TrafficPacer(double nominalLinkSpeed, uint16_t allPrio,
        uint16_t schedPrio, uint32_t grantMaxBytes,
        uint32_t maxAllowedInFlightBytes, PrioPaceMode paceMode)
    : actualLinkSpeed(nominalLinkSpeed * ACTUAL_TO_NOMINAL_RATE_RATIO)
    , nextGrantTime(SIMTIME_ZERO)
    , maxAllowedInFlightBytes(maxAllowedInFlightBytes)
    , grantMaxBytes(grantMaxBytes)
    , inflightUnschedPerPrio()
    , inflightSchedPerPrio()
    , totalOutstandingBytes(0)
    , unschedInflightBytes(0)
    , paceMode(paceMode)
    , allPrio(allPrio)
    , schedPrio(schedPrio)
{
    inflightUnschedPerPrio.resize(allPrio);
    inflightSchedPerPrio.resize(schedPrio);
}

TrafficPacer::~TrafficPacer()
{
}

/**
 * returns a lower bound on the next grant time
 */
simtime_t
TrafficPacer::grantSent(simtime_t currentTime, InboundMessage* grantedMsg,
    uint16_t prio, uint32_t grantSize)
{
    uint32_t grantedPktSizeOnWire = HomaPkt::getBytesOnWire(
        grantSize, PktType::SCHED_DATA);
    nextGrantTime = SimTime(1e-9 *
        (grantedPktSizeOnWire * 8.0 / actualLinkSpeed)) + currentTime;
    totalOutstandingBytes += grantedPktSizeOnWire;
    auto& prioInbndMap = inflightUnschedPerPrio[prio + schedPrio - allPrio - 1];
    auto inbndMsgOutbytesIter = prioInbndMap.find(grantedMsg);
    uint32_t outBytes = 0;
    if (inbndMsgOutbytesIter != prioInbndMap.end()) {
        outBytes = inbndMsgOutbytesIter->second;
        prioInbndMap.erase(inbndMsgOutbytesIter);
    }
    outBytes =+ HomaPkt::getBytesOnWire(grantSize, PktType::SCHED_DATA);
    auto retVal = prioInbndMap.insert(std::make_pair(grantedMsg, outBytes));
    ASSERT(retVal.second == true);
    return nextGrantTime;
}

bool
TrafficPacer::okToGrant(simtime_t currentTime, InboundMessage* msgToGrant,
    uint16_t &prio, uint32_t &grantSize)
{
    if (nextGrantTime > currentTime) {
        return false;
    }

    grantSize = std::min(msgToGrant->bytesToGrant, grantMaxBytes);
    uint32_t schedPktSizeOnWire =
        HomaPkt::getBytesOnWire(grantSize, PktType::SCHED_DATA);

    switch (paceMode) {
        case PrioPaceMode::NO_OVER_COMMIT:
            if ((totalOutstandingBytes + (int)schedPktSizeOnWire) >
                    (int)maxAllowedInFlightBytes) {
                return false;
            }
            return true;

        case PrioPaceMode::LOWEST_PRIO_POSSIBLE: {
            int msgToGrantInflightBytes = (int)(msgToGrant->bytesToReceive) -
                msgToGrant->bytesToGrant;
            int schedByteCap = maxAllowedInFlightBytes -
                (int)unschedInflightBytes - msgToGrantInflightBytes;
            if ((int)schedPktSizeOnWire > schedByteCap) {
                return false;
            }

            uint16_t prio = allPrio - 1;
            for (auto vecIter = inflightSchedPerPrio.rbegin();
                    vecIter != inflightSchedPerPrio.rend(); ++vecIter) {
                int sumPrioInflightBytes = 0;
                for (auto mapIter = vecIter->begin(); mapIter != vecIter->end();
                        ++mapIter) {
                    sumPrioInflightBytes += mapIter->second;
                }

                if (sumPrioInflightBytes + (int)schedPktSizeOnWire
                        <= schedByteCap) {
                    return true;
                }

                auto headMsg = vecIter->begin();
                if (headMsg->first->bytesToGrant <= msgToGrant->bytesToGrant) {
                    return false;
                }
                --prio;
            }
            return false;
        }
        case PrioPaceMode::PRIO_FROM_CBF:
            // TODO: Fill this block
            return false;

        default:
            return false;
    }

}

void
TrafficPacer::bytesArrived(InboundMessage* inbndMsg, uint32_t arrivedBytes,
    PktType recvPktType, uint16_t prio)
{
    totalOutstandingBytes -= arrivedBytes;
    if (paceMode == PrioPaceMode::NO_OVER_COMMIT)
        return;

    switch (recvPktType) {
        case PktType::REQUEST:
        case PktType::UNSCHED_DATA: {
            unschedInflightBytes -= arrivedBytes;
            auto& inbndMap = inflightUnschedPerPrio[prio];
            auto msgOutbytesIter = inbndMap.find(inbndMsg);
            ASSERT(msgOutbytesIter != inbndMap.end() &&
                msgOutbytesIter->second >= arrivedBytes);
            if ((msgOutbytesIter->second -= arrivedBytes) == 0) {
                inbndMap.erase(msgOutbytesIter);
            }
            break;
        }

        case PktType::SCHED_DATA: {
            auto& inbndMap = inflightUnschedPerPrio[prio + schedPrio - allPrio - 1];
            auto msgOutbytesIter = inbndMap.find(inbndMsg);
            ASSERT(msgOutbytesIter != inbndMap.end() &&
                msgOutbytesIter->second >= arrivedBytes);
            if ((msgOutbytesIter->second -= arrivedBytes) == 0) {
                inbndMap.erase(msgOutbytesIter);
            }
            break;
        }

        default:
            cRuntimeError("PktType %d: Invalid type of arrived bytes.",
                recvPktType);
            break;
    }
}

void
TrafficPacer::unschedPendingBytes(InboundMessage* inbndMsg,
    uint32_t committedBytes, PktType pendingPktType, uint16_t prio)
{
    totalOutstandingBytes += committedBytes;
    unschedInflightBytes += committedBytes;
    if (paceMode == PrioPaceMode::NO_OVER_COMMIT)
        return;

    switch (pendingPktType) {
        case PktType::REQUEST:
        case PktType::UNSCHED_DATA: {
            auto& inbndMsgMap = inflightUnschedPerPrio[prio];
            auto msgOutbytesIter = inbndMsgMap.find(inbndMsg);
            uint32_t outBytes = 0;
            if (msgOutbytesIter != inbndMsgMap.end()) {
                outBytes += msgOutbytesIter->second;
                inbndMsgMap.erase(msgOutbytesIter);
            }
            outBytes += committedBytes;
            auto retVal = inbndMsgMap.insert(std::make_pair(inbndMsg,outBytes));
            ASSERT(retVal.second == true);
            break;
        }
        default:
            cRuntimeError("PktType %d: Invalid type of incomping pkt",
                pendingPktType);
    }
}
