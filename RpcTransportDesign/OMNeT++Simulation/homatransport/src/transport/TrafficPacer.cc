/*
 * TrafficPacer.cc
 *
 *  Created on: Sep 18, 2015
 *      Author: neverhood
 */

#include <omnetpp.h>
#include "TrafficPacer.h"

TrafficPacer::TrafficPacer(PriorityResolver* prioRes,
        UnschedRateComputer* unschRateComp, HomaConfigDepot* homaConfig)
    : prioResolver(prioRes)
    , unschRateComp(unschRateComp)
    , homaConfig(homaConfig)
    , nextGrantTime(SIMTIME_ZERO)
    , inflightUnschedPerPrio()
    , inflightSchedPerPrio()
    , sumInflightUnschedPerPrio()
    , sumInflightSchedPerPrio()
    , totalOutstandingBytes(0)
    , unschedInflightBytes(0)
    , paceMode(strPrioPaceModeToEnum(homaConfig->schedPrioAssignMode))
    , lastGrantPrio(homaConfig->allPrio)
{
    inflightUnschedPerPrio.resize(homaConfig->allPrio);
    inflightSchedPerPrio.resize(homaConfig->allPrio);
    sumInflightUnschedPerPrio.resize(homaConfig->allPrio);
    std::fill(sumInflightUnschedPerPrio.begin(),
        sumInflightUnschedPerPrio.end(), 0);
    sumInflightSchedPerPrio.resize(homaConfig->allPrio);
    std::fill(sumInflightSchedPerPrio.begin(),
        sumInflightSchedPerPrio.end(), 0);
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
    nextGrantTime = SimTime(1e-9 * (grantedPktSizeOnWire * 8.0 /
        homaConfig->nicLinkSpeed)) + currentTime;
    return nextGrantTime;
}

HomaPkt*
TrafficPacer::getGrant(simtime_t currentTime, InboundMessage* msgToGrant,
    simtime_t &nextTimeToGrant)
{
    uint16_t prio = 0;
    uint32_t grantSize =
        std::min(msgToGrant->bytesToGrant, homaConfig->grantMaxBytes);
    uint32_t grantedPktSizeOnWire =
        HomaPkt::getBytesOnWire(grantSize, PktType::SCHED_DATA);
    int schedInflightByteCap = (int)(homaConfig->maxOutstandingRecvBytes *
        (1-unschRateComp->getAvgUnschRate(currentTime)));
    uint16_t adaptivePrioSearchRange = homaConfig->adaptiveSchedPrioLevels;
    uint16_t prioSMF = homaConfig->allPrio-1;
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
            prio = prioResolver->getSchedPktPrio(
                prioPace2PrioResolution(paceMode), msgToGrant);
            sumInflightSchedPerPrio[prio] +=
                grantedPktSizeOnWire;
            return msgToGrant->prepareGrant(grantSize, prio);
        }

        case PrioPaceMode::SIMULATED_SRBF:
        case PrioPaceMode::SMF_CBF_BASED:
        case PrioPaceMode::SMF_LAST_CAP_CBF:
            if (msgToGrant->bytesToGrant <
                    (uint32_t)homaConfig->cbfCapMsgSize) {
                adaptivePrioSearchRange = inflightSchedPerPrio.size();
                // last 1 RTT remaining bytes will get priority like the
                // unscheduled.
                prioSMF = prioResolver->getSchedPktPrio(
                    prioPace2PrioResolution(paceMode), msgToGrant);
            }

        case PrioPaceMode::ADAPTIVE_LOWEST_PRIO_POSSIBLE: {
            ASSERT(msgToGrant->bytesToGrant >= 0);
            int schedByteCap =
                schedInflightByteCap - (int)totalOutstandingBytes;

            prio = homaConfig->allPrio - 1;
            for (auto vecIter = inflightSchedPerPrio.rbegin();
                    vecIter != inflightSchedPerPrio.rbegin() +
                    adaptivePrioSearchRange; ++vecIter) {

                if ((int)grantedPktSizeOnWire <= schedByteCap) {
                    
                    prio = std::min(prioSMF, prio);
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
                    for (auto schVecIter = inflightSchedPerPrio.begin();
                            schVecIter != inflightSchedPerPrio.end();
                            ++schVecIter) {
                        uint16_t schedInd =
                            schVecIter - inflightSchedPerPrio.begin();
                        uint16_t prioOfInd = schedIndToPrio(schedInd);
                        auto inbndMsgOutbytesIter= schVecIter->find(msgToGrant);

                        if (inbndMsgOutbytesIter != schVecIter->end()) {
                            if (prioOfInd == prio) {
                                prioSchedVec.push_back(std::make_pair(schedInd,
                                    inbndMsgOutbytesIter->second +
                                    grantedPktSizeOnWire));
                            } else {
                                prioSchedVec.push_back(std::make_pair(schedInd,
                                    inbndMsgOutbytesIter->second));
                            }
                            schVecIter->erase(inbndMsgOutbytesIter);
                        } else if (prioOfInd == prio) {
                            prioSchedVec.push_back(
                                std::make_pair(schedInd, grantedPktSizeOnWire));
                        }
                    }

                    // Since the unsched mapped message struct is also sorted
                    // based on the bytesToGrant, we also need to update that
                    // struct if necessary.
                    std::vector<std::pair<uint16, uint32_t>>prioUnschedVec = {};
                    uint16_t unschdInd;
                    for (auto unschVecIter = inflightUnschedPerPrio.begin();
                            unschVecIter != inflightUnschedPerPrio.end();
                            ++unschVecIter) {
                        auto inbndMsgOutbytesIter =
                            unschVecIter->find(msgToGrant);
                        if (inbndMsgOutbytesIter != unschVecIter->end()) {
                            unschdInd =
                                unschVecIter - inflightUnschedPerPrio.begin();
                            prioUnschedVec.push_back(std::make_pair(unschdInd,
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
                
                uint16_t uind = prioToUnschedInd(prio);
                auto headUnschedMsg = inflightUnschedPerPrio[uind].begin();
                if (headUnschedMsg != inflightUnschedPerPrio[uind].end()) {
                    if ((headUnschedMsg->first->bytesToGrant <
                        msgToGrant->bytesToGrant) ||
                        (headUnschedMsg->first->bytesToGrant ==
                        msgToGrant->bytesToGrant &&
                        headUnschedMsg->first->msgSize <= msgToGrant->msgSize))
                    {
                        return NULL;
                    }
                }

                int sumPrioInflightBytes = 0;
                for (auto mapIter = vecIter->begin(); mapIter != vecIter->end();
                        ++mapIter) {
                    sumPrioInflightBytes += mapIter->second;
                }
                for (auto unschedMapIter = inflightUnschedPerPrio[uind].begin();
                        unschedMapIter != inflightUnschedPerPrio[uind].end();
                        ++unschedMapIter) {
                    sumPrioInflightBytes += unschedMapIter->second;
                }
                --prio;
                schedByteCap += sumPrioInflightBytes;
            }
            return NULL;
        }
        default:
            throw cRuntimeError("PrioPaceMode %d: Invalid value.", paceMode);
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
        case PrioPaceMode::SIMULATED_SRBF:
        case PrioPaceMode::SMF_CBF_BASED:
        case PrioPaceMode::SMF_LAST_CAP_CBF:
            switch (recvPktType) {
                case PktType::REQUEST:
                case PktType::UNSCHED_DATA: {
                    unschedInflightBytes -= arrivedBytesOnWire;
                    sumInflightUnschedPerPrio[prio] -= arrivedBytesOnWire;
                    uint16_t uind = prioToUnschedInd(prio);
                    auto& inbndMap = inflightUnschedPerPrio[uind];
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
                    uint16_t sind = prioToSchedInd(prio);
                    auto& inbndMap = inflightSchedPerPrio[sind];
                    auto msgOutbytesIter = inbndMap.find(inbndMsg);
                    ASSERT(msgOutbytesIter != inbndMap.end() &&
                        msgOutbytesIter->second >= arrivedBytesOnWire);
                    if ((msgOutbytesIter->second -= arrivedBytesOnWire) == 0) {
                        inbndMap.erase(msgOutbytesIter);
                    }
                    break;
                }

                default:
                    throw cRuntimeError("PktType %d: Invalid type of arrived bytes.",
                        recvPktType);
            }
            return;
        default:
            throw cRuntimeError("PrioPaceMode %d: Invalid value.", paceMode);
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

        case PrioPaceMode::ADAPTIVE_LOWEST_PRIO_POSSIBLE:
        case PrioPaceMode::SIMULATED_SRBF:
        case PrioPaceMode::SMF_CBF_BASED:
        case PrioPaceMode::SMF_LAST_CAP_CBF:
            switch (pendingPktType) {
                case PktType::REQUEST:
                case PktType::UNSCHED_DATA: {
                    uint16_t uind = prioToUnschedInd(prio);
                    auto& inbndMsgMap = inflightUnschedPerPrio[uind];
                    auto msgOutbytesIter = inbndMsgMap.find(inbndMsg);
                    uint32_t outBytes = 0;
                    if (msgOutbytesIter != inbndMsgMap.end()) {
                        outBytes += msgOutbytesIter->second;
                        inbndMsgMap.erase(msgOutbytesIter);
                    }
                    outBytes += committedBytesOnWire;
                    auto retVal =
                        inbndMsgMap.insert(std::make_pair(inbndMsg, outBytes));
                    ASSERT(retVal.second == true);
                    break;
                }
                default:
                    throw cRuntimeError("PktType %d: Invalid type of incomping pkt",
                        pendingPktType);
            }
            return;
        default:
            throw cRuntimeError("PrioPaceMode %d: Invalid value.", paceMode);
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
    } else if (strcmp(prioPaceMode, "SIMULATED_SRBF") == 0) {
        return PrioPaceMode::SIMULATED_SRBF;
    } else if (strcmp(prioPaceMode, "SMF_CBF_BASED") == 0) {
        return PrioPaceMode::SMF_CBF_BASED;
    } else if (strcmp(prioPaceMode, "SMF_LAST_CAP_CBF") == 0) {
        return PrioPaceMode::SMF_LAST_CAP_CBF;
    }

    throw cRuntimeError("Unknown value for paceMode: %s", prioPaceMode);
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
        case PrioPaceMode::SIMULATED_SRBF:
            return PriorityResolver::PrioResolutionMode::SIMULATED_SRBF;
        case PrioPaceMode::SMF_CBF_BASED:
            return PriorityResolver::PrioResolutionMode::SMF_CBF_BASED;
        case PrioPaceMode::SMF_LAST_CAP_CBF:
            return PriorityResolver::PrioResolutionMode::SMF_LAST_CAP_CBF;
        default:
            throw cRuntimeError( "PrioPaceMode %d has no match in PrioResolutionMode",
                prioPaceMode);
    }
    return PriorityResolver::PrioResolutionMode::INVALID_PRIO_MODE;
}

