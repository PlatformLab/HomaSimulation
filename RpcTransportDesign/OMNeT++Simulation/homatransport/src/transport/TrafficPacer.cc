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
        uint32_t maxAllowedInFlightBytes, const char* prioPaceMode)
    : actualLinkSpeed(nominalLinkSpeed * ACTUAL_TO_NOMINAL_RATE_RATIO)
    , nextGrantTime(SIMTIME_ZERO)
    , maxAllowedInFlightBytes(maxAllowedInFlightBytes)
    , grantMaxBytes(grantMaxBytes)
    , inflightUnschedPerPrio()
    , inflightSchedPerPrio()
    , totalOutstandingBytes(0)
    , unschedInflightBytes(0)
    , paceMode(strPrioPaceModeToEnum(prioPaceMode))
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
TrafficPacer::getNextGrantTime(simtime_t currentTime,
    uint32_t grantedPktSizeOnWire)
{
    nextGrantTime = SimTime(1e-9 *
        (grantedPktSizeOnWire * 8.0 / actualLinkSpeed)) + currentTime;
    return nextGrantTime;
}

HomaPkt*
TrafficPacer::getGrant(simtime_t currentTime, InboundMessage* msgToGrant,
    simtime_t &nextTimeToGrant)
{
    uint16_t prio = 0;
    if (nextGrantTime > currentTime) {
        return NULL;
    }

    uint32_t grantSize = std::min(msgToGrant->bytesToGrant, grantMaxBytes);
    uint32_t grantedPktSizeOnWire =
        HomaPkt::getBytesOnWire(grantSize, PktType::SCHED_DATA);

    switch (paceMode) {
        case PrioPaceMode::NO_OVER_COMMIT:
            if ((totalOutstandingBytes + (int)grantedPktSizeOnWire) >
                    (int)maxAllowedInFlightBytes) {
                return NULL;
            }

            // We can prepare and return a grant
            nextTimeToGrant = getNextGrantTime(currentTime, grantedPktSizeOnWire);
            totalOutstandingBytes += grantedPktSizeOnWire;
            return msgToGrant->prepareGrant(grantSize, prio);

        case PrioPaceMode::LOWEST_PRIO_POSSIBLE: {
            ASSERT(msgToGrant >= 0);
            int schedByteCap = maxAllowedInFlightBytes - (int)unschedInflightBytes
                - (int)(HomaPkt::getBytesOnWire(msgToGrant->schedBytesInFlight(),
                PktType::SCHED_DATA));
            if ((int)grantedPktSizeOnWire > schedByteCap) {
                return NULL;
            }

            prio = allPrio - 1;
            for (auto vecIter = inflightSchedPerPrio.rbegin();
                    vecIter != inflightSchedPerPrio.rend(); ++vecIter) {
                int sumPrioInflightBytes = 0;
                for (auto mapIter = vecIter->begin(); mapIter != vecIter->end();
                        ++mapIter) {
                    if (mapIter->first != msgToGrant) {
                        sumPrioInflightBytes += mapIter->second;
                    }
                }

                if (sumPrioInflightBytes + (int)grantedPktSizeOnWire <=
                        schedByteCap) {

                    // We can send a grant here.
                    // First find this message in scheduled mapped messages of
                    // this module and remove it from the map. 
                    auto inbndMsgOutbytesIter = vecIter->find(msgToGrant);
                    uint32_t outBytes = 0;
                    if (inbndMsgOutbytesIter != vecIter->end()) {
                        outBytes += inbndMsgOutbytesIter->second;
                        vecIter->erase(inbndMsgOutbytesIter);
                    }
                    outBytes += grantedPktSizeOnWire;

                    // Since the unsched mapped message struct is also sorted
                    // based on the bytesToGrant, we also need to update that
                    // struct if necessary.
                    std::vector<std::pair<uint16, uint32_t>>prioUnschedVec = {};
                    for (auto unschVecIter = inflightUnschedPerPrio.begin(); 
                            unschVecIter != inflightUnschedPerPrio.end();
                            ++unschVecIter) {
                        inbndMsgOutbytesIter = unschVecIter->find(msgToGrant);
                        if (inbndMsgOutbytesIter != unschVecIter->end()) {
                            prioUnschedVec.push_back(
                                std::make_pair(
                                unschVecIter - inflightUnschedPerPrio.begin(),
                                inbndMsgOutbytesIter->second));
                            unschVecIter->erase(inbndMsgOutbytesIter);
                        }
                    }

                    // prepare the return values of this method (ie. grantPkt
                    // and nextTimeToGrant)
                    nextTimeToGrant =
                        getNextGrantTime(currentTime, grantedPktSizeOnWire);
                    HomaPkt* grantPkt =
                        msgToGrant->prepareGrant(grantSize, prio);
                    
                    // inserted the updated msgToGrant into the scheduled
                    // message map struct of this module.
                    auto retVal =
                        vecIter->insert(std::make_pair(msgToGrant, outBytes));
                    ASSERT(retVal.second == true);

                    // inserted the updated msgToGrant into the unscheduled
                    // message map struct of this module.
                    for (auto elem : prioUnschedVec) {
                        retVal = inflightUnschedPerPrio[elem.first].insert(
                            std::make_pair(msgToGrant, elem.second));
                        ASSERT(retVal.second == true);
                    }
                    
                    totalOutstandingBytes += grantedPktSizeOnWire;
                    return grantPkt;
                }

                auto headMsg = vecIter->begin();
                if (headMsg->first->bytesToGrant <= msgToGrant->bytesToGrant) {
                    return NULL;
                }
                --prio;
            }
            return NULL;
        }
        case PrioPaceMode::PRIO_FROM_CBF:
            // TODO: Fill this block
            return NULL;

        default:
            return NULL;
    }

}

void
TrafficPacer::bytesArrived(InboundMessage* inbndMsg, uint32_t arrivedBytes,
    PktType recvPktType, uint16_t prio)
{
    uint32_t arrivedBytesOnWire = HomaPkt::getBytesOnWire(arrivedBytes,
        recvPktType);
    totalOutstandingBytes -= arrivedBytesOnWire;
    if (paceMode == PrioPaceMode::NO_OVER_COMMIT) {
        switch (recvPktType) {
            case PktType::REQUEST:
            case PktType::UNSCHED_DATA:
                unschedInflightBytes -= arrivedBytesOnWire;
                break; 
            default:
                break;
        }
        return;
    }

    switch (recvPktType) {
        case PktType::REQUEST:
        case PktType::UNSCHED_DATA: {
            unschedInflightBytes -= arrivedBytesOnWire;
            auto& inbndMap = inflightUnschedPerPrio[prio];
            auto msgOutbytesIter = inbndMap.find(inbndMsg);
            ASSERT ((msgOutbytesIter != inbndMap.end() &&
                msgOutbytesIter->second >= arrivedBytesOnWire));
            if ((msgOutbytesIter->second -= arrivedBytesOnWire) == 0) {
                inbndMap.erase(msgOutbytesIter);
            }
            break;
        }

        case PktType::SCHED_DATA: {
            auto& inbndMap = inflightSchedPerPrio[prio + schedPrio - allPrio];
            auto msgOutbytesIter = inbndMap.find(inbndMsg);
            ASSERT(msgOutbytesIter != inbndMap.end() &&
                msgOutbytesIter->second >= arrivedBytesOnWire);
            if ((msgOutbytesIter->second -= arrivedBytesOnWire) == 0) {
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
    uint32_t committedBytesOnWire =
        HomaPkt::getBytesOnWire(committedBytes, pendingPktType);
    totalOutstandingBytes += committedBytesOnWire;
    unschedInflightBytes += committedBytesOnWire;
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
            outBytes += committedBytesOnWire;
            auto retVal = inbndMsgMap.insert(std::make_pair(inbndMsg,outBytes));
            ASSERT(retVal.second == true);
            break;
        }
        default:
            cRuntimeError("PktType %d: Invalid type of incomping pkt",
                pendingPktType);
    }
}

TrafficPacer::PrioPaceMode
TrafficPacer::strPrioPaceModeToEnum(const char* prioPaceMode)
{
    if (strcmp(prioPaceMode, "NO_OVER_COMMIT") == 0) {
        return PrioPaceMode::NO_OVER_COMMIT;
    } else if (strcmp(prioPaceMode, "LOWEST_PRIO_POSSIBLE") == 0) {
        return PrioPaceMode::LOWEST_PRIO_POSSIBLE;
    } else if (strcmp(prioPaceMode, "PRIO_FROM_CBF") == 0) {
        return PrioPaceMode::PRIO_FROM_CBF;
    }
    cRuntimeError("Unknown value for paceMode: %s", prioPaceMode);
    return PrioPaceMode::INVALIDE_MODE; 
}
