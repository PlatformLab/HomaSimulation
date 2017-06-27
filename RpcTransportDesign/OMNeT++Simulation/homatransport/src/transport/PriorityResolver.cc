/*
 * PriorityResolver.cc
 *
 *  Created on: Dec 23, 2015
 *      Author: behnamm
 */

#include "PriorityResolver.h"

PriorityResolver::PriorityResolver(HomaConfigDepot* homaConfig,
        WorkloadEstimator* distEstimator)
    : cdf(&distEstimator->cdfFromFile)
    , cbf(&distEstimator->cbfFromFile)
    , cbfLastCapBytes(&distEstimator->cbfLastCapBytesFromFile)
    , remainSizeCbf(&distEstimator->remainSizeCbf)
    , prioCutOffs()
    , distEstimator(distEstimator)
    , prioResMode()
    , homaConfig(homaConfig)
{
    distEstimator->getCbfFromCdf(distEstimator->cdfFromFile,
        homaConfig->cbfCapMsgSize, homaConfig->boostTailBytesPrio);
    distEstimator->getRemainSizeCdfCbf(distEstimator->cdfFromFile,
        homaConfig->cbfCapMsgSize, homaConfig->boostTailBytesPrio);
    prioResMode = strPrioModeToInt(homaConfig->unschedPrioResolutionMode);
    setPrioCutOffs();
}

uint16_t
PriorityResolver::getMesgPrio(uint32_t msgSize)
{
    size_t mid, high, low;
    low = 0;
    high = prioCutOffs.size() - 1;
    while(low < high) {
        mid = (high + low) / 2;
        if (msgSize <= prioCutOffs.at(mid)) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return high;
}

std::vector<uint16_t>
PriorityResolver::getUnschedPktsPrio(const OutboundMessage* outbndMsg)
{
    uint32_t msgSize = outbndMsg->msgSize;
    switch (prioResMode) {
        case PrioResolutionMode::FIXED_UNSCHED: {
            std::vector<uint16_t> unschedPktsPrio(
                outbndMsg->reqUnschedDataVec.size(), 0);
            return unschedPktsPrio;
        }
        case PrioResolutionMode::EXPLICIT:
        case PrioResolutionMode::STATIC_CBF_GRADUATED: {
            std::vector<uint16_t> unschedPktsPrio;
            for (auto& pkt : outbndMsg->reqUnschedDataVec) {
                unschedPktsPrio.push_back(getMesgPrio(msgSize));
                ASSERT(msgSize >= pkt);
                msgSize -= pkt;
            }
            return unschedPktsPrio;
        }
        case PrioResolutionMode::STATIC_CDF_UNIFORM:
        case PrioResolutionMode::STATIC_CBF_UNIFORM: {
            uint16_t prio = getMesgPrio(msgSize);
            std::vector<uint16_t> unschedPktsPrio(
                outbndMsg->reqUnschedDataVec.size(), prio);
            return unschedPktsPrio;
        }
        default:
            throw cRuntimeError("Invalid priority mode for : prioMode(%d) "
            "for unscheduled packets.", prioResMode);
    }

}

uint16_t
PriorityResolver::getSchedPktPrio(const InboundMessage* inbndMsg)
{
    uint32_t msgSize = inbndMsg->msgSize;
    uint32_t bytesToGrant = inbndMsg->bytesToGrant;
    uint32_t bytesToGrantOnWire = HomaPkt::getBytesOnWire(bytesToGrant,
        PktType::SCHED_DATA);                                 
    uint32_t bytesTreatedUnsched = homaConfig->boostTailBytesPrio;
    switch (prioResMode) {
        case PrioResolutionMode::EXPLICIT:
        case PrioResolutionMode::STATIC_CBF_GRADUATED: {
            if (bytesToGrantOnWire < bytesTreatedUnsched) {
                return getMesgPrio(bytesToGrant);
            } else {
                return homaConfig->allPrio-1;
            }
        }
        case PrioResolutionMode::STATIC_CDF_UNIFORM: {
        case PrioResolutionMode::STATIC_CBF_UNIFORM:
            if (bytesToGrantOnWire < bytesTreatedUnsched) {
                return getMesgPrio(msgSize);
            } else {
                return homaConfig->allPrio-1;
            }

        }
        default:
            throw cRuntimeError("Invalid priority mode: prioMode(%d) for"
                " scheduled packets", prioResMode);
    }
    return homaConfig->allPrio-1;
}

void
PriorityResolver::recomputeCbf(uint32_t cbfCapMsgSize,
    uint32_t boostTailBytesPrio)
{
    distEstimator->getCbfFromCdf(distEstimator->cdfFromFile,
        cbfCapMsgSize, boostTailBytesPrio);
    distEstimator->getRemainSizeCdfCbf(distEstimator->cdfFromFile,
        cbfCapMsgSize, boostTailBytesPrio);
    setPrioCutOffs();
}

void
PriorityResolver::setPrioCutOffs()
{
    prioCutOffs.clear();
    const WorkloadEstimator::CdfVector* vecToUse = NULL;
    if (prioResMode == PrioResolutionMode::EXPLICIT) {
        prioCutOffs = homaConfig->explicitUnschedPrioCutoff;
        if (prioCutOffs.back() != UINT32_MAX) {
            prioCutOffs.push_back(UINT32_MAX);
        }
        return;
    }

    if (prioResMode == PrioResolutionMode::STATIC_CDF_UNIFORM) {
        vecToUse = cdf;
    } else if (prioResMode == PrioResolutionMode::STATIC_CBF_UNIFORM) {
        vecToUse = cbfLastCapBytes;
    } else if (prioResMode == PrioResolutionMode::STATIC_CBF_GRADUATED) {
        vecToUse = remainSizeCbf;
    } else {
        throw cRuntimeError("Invalid priority mode: prioMode(%d) for"
            " scheduled packets", prioResMode);
    }

    ASSERT(isEqual(vecToUse->at(vecToUse->size() - 1).second, 1.00));

    double probMax = 1.0;
    double fac = homaConfig->unschedPrioUsageWeight;
    double probStep;
    if (fac == 1) {
        probStep = probMax / homaConfig->prioResolverPrioLevels;
    } else {
        probStep = probMax * (1.0 - fac) /
            (1 - pow(fac, (int)(homaConfig->prioResolverPrioLevels)));
    }
    size_t i = 0;
    uint32_t prevCutOffSize = UINT32_MAX;
    for (double prob = probStep; isLess(prob, probMax); prob += probStep) {
        probStep *= fac;
        for (; i < vecToUse->size(); i++) {
            if (vecToUse->at(i).first == prevCutOffSize) {
                // Do not add duplicate sizes to cutOffSizes vector
                continue;
            }
            if (vecToUse->at(i).second >= prob) {
                prioCutOffs.push_back(vecToUse->at(i).first);
                prevCutOffSize = vecToUse->at(i).first;
                break;
            }
        }
    }
    prioCutOffs.push_back(UINT32_MAX);
}

PriorityResolver::PrioResolutionMode
PriorityResolver::strPrioModeToInt(const char* prioResMode)
{
    if (strcmp(prioResMode, "STATIC_CDF_UNIFORM") == 0) {
        return PrioResolutionMode::STATIC_CDF_UNIFORM;
    } else if (strcmp(prioResMode, "STATIC_CBF_UNIFORM") == 0) {
        return PrioResolutionMode::STATIC_CBF_UNIFORM;
    } else if (strcmp(prioResMode, "STATIC_CBF_GRADUATED") == 0) {
        return PrioResolutionMode::STATIC_CBF_GRADUATED;
    } else if (strcmp(prioResMode, "EXPLICIT") == 0) {
        return PrioResolutionMode::EXPLICIT;
    } else if (strcmp(prioResMode, "FIXED_UNSCHED") == 0) {
        return PrioResolutionMode::STATIC_CBF_GRADUATED;
    } else {
        return PrioResolutionMode::INVALID_PRIO_MODE;
    }
}

void
PriorityResolver::printCbfCdf(WorkloadEstimator::CdfVector* vec)
{
    for (auto& elem:*vec) {
        std::cout << elem.first << " : " << elem.second << std::endl;
    }
}
