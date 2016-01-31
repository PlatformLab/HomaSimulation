/*
 * TrafficPacer.cc
 *
 *  Created on: Sep 18, 2015
 *      Author: neverhood
 */

#include <omnetpp.h>
#include "TrafficPacer.h"

TrafficPacer::TrafficPacer(PriorityResolver* prioRes,
        UnschedRateComputer* unschRateComp, double nominalLinkSpeed,
        uint16_t allPrio, uint16_t schedPrio, uint32_t grantMaxBytes,
        uint32_t maxAllowedInFlightBytes, const char* prioPaceMode)
    : prioResolver(prioRes)
    , unschRateComp(unschRateComp)
    , actualLinkSpeed(nominalLinkSpeed * ACTUAL_TO_NOMINAL_RATE_RATIO)
    , nextGrantTime(SIMTIME_ZERO)
    , maxAllowedInFlightBytes(maxAllowedInFlightBytes)
    , grantMaxBytes(grantMaxBytes)
    , inflightUnschedPerPrio()
    , inflightSchedPerPrio()
    , sumInflightUnschedPerPrio()
    , sumInflightSchedPerPrio()
    , totalOutstandingBytes(0)
    , unschedInflightBytes(0)
    , paceMode(strPrioPaceModeToEnum(prioPaceMode))
    , allPrio(allPrio)
    , schedPrio(schedPrio)
    , lastGrantPrio(allPrio)
{
    inflightUnschedPerPrio.resize(allPrio);
    inflightSchedPerPrio.resize(schedPrio);
    sumInflightUnschedPerPrio.resize(allPrio);
    std::fill(sumInflightUnschedPerPrio.begin(), sumInflightUnschedPerPrio.end(), 0);
    sumInflightSchedPerPrio.resize(allPrio);
    std::fill(sumInflightSchedPerPrio.begin(), sumInflightSchedPerPrio.end(), 0);
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
    uint32_t grantSize = std::min(msgToGrant->bytesToGrant, grantMaxBytes);
    uint32_t grantedPktSizeOnWire =
        HomaPkt::getBytesOnWire(grantSize, PktType::SCHED_DATA);
    int schedInflightByteCap = (int)(maxAllowedInFlightBytes *
        (1-unschRateComp->getAvgUnschRate(currentTime)));

    switch (paceMode) {
        case PrioPaceMode::FIXED:
        case PrioPaceMode::STATIC_FROM_CBF:
        case PrioPaceMode::STATIC_FROM_CDF: {
            if (nextGrantTime > currentTime) {
                return NULL;
            }

            if ((totalOutstandingBytes + (int)grantedPktSizeOnWire) >
                    schedInflightByteCap) {
                return NULL;
            }

            // We can prepare and return a grant
            lastGrantPrio = prio;
            nextTimeToGrant =
                getNextGrantTime(currentTime, grantedPktSizeOnWire);
            totalOutstandingBytes += grantedPktSizeOnWire;
            prio = prioResolver->getPrioForPkt(prioPace2PrioResolution(paceMode),
                msgToGrant->msgSize, PktType::SCHED_DATA);
            sumInflightSchedPerPrio[prio] +=
                grantedPktSizeOnWire;
            return msgToGrant->prepareGrant(grantSize, prio);
        }

        case PrioPaceMode::ADAPTIVE_LOWEST_PRIO_POSSIBLE: {
            ASSERT(msgToGrant->bytesToGrant >= 0);
            int schedByteCap =
                schedInflightByteCap - (int)totalOutstandingBytes;

            prio = allPrio - 1;
            for (auto vecIter = inflightSchedPerPrio.rbegin();
                    vecIter != inflightSchedPerPrio.rend(); ++vecIter) {

                if ((int)grantedPktSizeOnWire <= schedByteCap) {
                    // If the privious grant was sent on same or higher (lower
                    // value) priority, the we should check that we've passed
                    // nextGrantTime
                    if (prio >= lastGrantPrio && nextGrantTime > currentTime) {
                        return NULL;
                    }
                    lastGrantPrio = prio;

                    // We can now send a grant here at the current prio.
                    // First find this current message in scheduled mapped
                    // messages of this module and remove it from all maps but
                    // keep track of the maps.
                    std::vector<std::pair<uint16, uint32_t>>prioSchedVec = {};
                    auto inbndMsgOutbytesIter = vecIter->find(msgToGrant);
                    if (inbndMsgOutbytesIter == vecIter->end()) {
                        prioSchedVec.push_back(std::make_pair(
                            prio + schedPrio - allPrio, grantedPktSizeOnWire));
                    } else {
                        prioSchedVec.push_back(std::make_pair(
                            prio + schedPrio - allPrio,
                            inbndMsgOutbytesIter->second+grantedPktSizeOnWire));
                        vecIter->erase(inbndMsgOutbytesIter);
                    }

                    for (auto schVecIter = inflightSchedPerPrio.begin();
                            schVecIter != inflightSchedPerPrio.end();
                            ++schVecIter) {
                        inbndMsgOutbytesIter = schVecIter->find(msgToGrant);
                        if (inbndMsgOutbytesIter != schVecIter->end()) {
                            prioSchedVec.push_back(std::make_pair(
                                schVecIter - inflightSchedPerPrio.begin(),
                                inbndMsgOutbytesIter->second));
                            schVecIter->erase(inbndMsgOutbytesIter);
                        }
                    }

                    // Since the unsched mapped message struct is also sorted
                    // based on the bytesToGrant, we also need to update that
                    // struct if necessary.
                    std::vector<std::pair<uint16, uint32_t>>prioUnschedVec = {};
                    for (auto unschVecIter = inflightUnschedPerPrio.begin();
                            unschVecIter != inflightUnschedPerPrio.end();
                            ++unschVecIter) {
                        inbndMsgOutbytesIter = unschVecIter->find(msgToGrant);
                        if (inbndMsgOutbytesIter != unschVecIter->end()) {
                            prioUnschedVec.push_back(std::make_pair(
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
                    ASSERT(!prioSchedVec.empty());
                    for (auto elem : prioSchedVec) {
                        auto retVal = inflightSchedPerPrio[elem.first].insert(
                            std::make_pair(msgToGrant, elem.second));
                        ASSERT(retVal.second == true);
                    }

                    // inserted the updated msgToGrant into the unscheduled
                    // message map struct of this module.
                    for (auto elem : prioUnschedVec) {
                        auto retVal = inflightUnschedPerPrio[elem.first].insert(
                            std::make_pair(msgToGrant, elem.second));
                        ASSERT(retVal.second == true);
                    }

                    totalOutstandingBytes += grantedPktSizeOnWire;
                    sumInflightSchedPerPrio[prio] +=
                        grantedPktSizeOnWire;
                    return grantPkt;
                }

                auto headSchedMsg = vecIter->begin();
                if (headSchedMsg != vecIter->end()) {
                    if ((headSchedMsg->first->bytesToGrant <
                        msgToGrant->bytesToGrant) ||
                        (headSchedMsg->first->bytesToGrant ==
                        msgToGrant->bytesToGrant &&
                        headSchedMsg->first->msgSize <= msgToGrant->msgSize)) {
                        return NULL;
                    }
                }

                auto headUnschedMsg = inflightUnschedPerPrio[prio].begin();
                if (headUnschedMsg != inflightUnschedPerPrio[prio].end()) {
                    if ((headUnschedMsg->first->bytesToGrant <
                        msgToGrant->bytesToGrant) ||
                        (headUnschedMsg->first->bytesToGrant ==
                        msgToGrant->bytesToGrant &&
                        headUnschedMsg->first->msgSize <= msgToGrant->msgSize)) {
                        return NULL;
                    }
                }

                int sumPrioInflightBytes = 0;
                for (auto mapIter = vecIter->begin(); mapIter != vecIter->end();
                        ++mapIter) {
                    sumPrioInflightBytes += mapIter->second;
                }
                for (auto unschedMapIter = inflightUnschedPerPrio[prio].begin();
                        unschedMapIter != inflightUnschedPerPrio[prio].end();
                        ++unschedMapIter) {
                    sumPrioInflightBytes += unschedMapIter->second;
                }
                --prio;
                schedByteCap += sumPrioInflightBytes;
            }
            return NULL;
        }
        default:
            cRuntimeError("PrioPaceMode %d: Invalid value.", paceMode);
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
    switch (paceMode) {
        case PrioPaceMode::FIXED:
        case PrioPaceMode::STATIC_FROM_CBF:
        case PrioPaceMode::STATIC_FROM_CDF:
            switch (recvPktType) {
                case PktType::REQUEST:
                case PktType::UNSCHED_DATA:
                    unschedInflightBytes -= arrivedBytesOnWire;
                    sumInflightUnschedPerPrio[prio] -= arrivedBytesOnWire;
                    break;
                case PktType::SCHED_DATA:
                    sumInflightSchedPerPrio[prio] -=
                        arrivedBytesOnWire;
                default:
                    break;
            }
            return;

        case PrioPaceMode::ADAPTIVE_LOWEST_PRIO_POSSIBLE:
            switch (recvPktType) {
                case PktType::REQUEST:
                case PktType::UNSCHED_DATA: {
                    unschedInflightBytes -= arrivedBytesOnWire;
                    sumInflightUnschedPerPrio[prio] -= arrivedBytesOnWire;
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
                    sumInflightSchedPerPrio[prio] -=
                        arrivedBytesOnWire;
                    auto& inbndMap =
                        inflightSchedPerPrio[prio + schedPrio - allPrio];
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
        default:
            cRuntimeError("PrioPaceMode %d: Invalid value.", paceMode);
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
    sumInflightUnschedPerPrio[prio] += committedBytesOnWire;
    switch (paceMode) {
        case PrioPaceMode::FIXED:
        case PrioPaceMode::STATIC_FROM_CBF:
        case PrioPaceMode::STATIC_FROM_CDF:
            return;

        case PrioPaceMode:: ADAPTIVE_LOWEST_PRIO_POSSIBLE:
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
                    auto retVal =
                        inbndMsgMap.insert(std::make_pair(inbndMsg,outBytes));
                    ASSERT(retVal.second == true);
                    break;
                }
                default:
                    cRuntimeError("PktType %d: Invalid type of incomping pkt",
                        pendingPktType);
            }
        default:
            cRuntimeError("PrioPaceMode %d: Invalid value.", paceMode);
    }
}

TrafficPacer::PrioPaceMode
TrafficPacer::strPrioPaceModeToEnum(const char* prioPaceMode)
{
    if (strcmp(prioPaceMode, "FIXED") == 0) {
        return PrioPaceMode::FIXED;
    } else if (strcmp(prioPaceMode, "ADAPTIVE_LOWEST_PRIO_POSSIBLE") == 0) {
        return PrioPaceMode::ADAPTIVE_LOWEST_PRIO_POSSIBLE;
    } else if (strcmp(prioPaceMode, "STATIC_FROM_CBF") == 0) {
        return PrioPaceMode::STATIC_FROM_CBF;
    } else if (strcmp(prioPaceMode, "STATIC_FROM_CDF") == 0) {
        return PrioPaceMode::STATIC_FROM_CDF;
    }
    cRuntimeError("Unknown value for paceMode: %s", prioPaceMode);
    return PrioPaceMode::INVALIDE_MODE;
}

PriorityResolver::PrioResolutionMode
TrafficPacer::prioPace2PrioResolution(TrafficPacer::PrioPaceMode prioPaceMode)
{
    switch (prioPaceMode) {
        case PrioPaceMode::FIXED:
            return PriorityResolver::PrioResolutionMode::FIXED_SCHED;
        case PrioPaceMode::STATIC_FROM_CBF:
            return PriorityResolver::PrioResolutionMode::STATIC_FROM_CBF;
        case PrioPaceMode::STATIC_FROM_CDF:
            return PriorityResolver::PrioResolutionMode::STATIC_FROM_CDF;
        default:
            cRuntimeError( "PrioPaceMode %d has no match in PrioResolutionMode",
                prioPaceMode);
    }
    return PriorityResolver::PrioResolutionMode::INVALID_PRIO_MODE;
}

