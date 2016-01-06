/*
 * PriorityResolver.cc
 *
 *  Created on: Dec 23, 2015
 *      Author: behnamm
 */

#include "PriorityResolver.h"

PriorityResolver::PriorityResolver(const uint32_t numPrios,
        const WorkloadEstimator* distEstimator)
    : numPrios(numPrios)
    , cdf(&distEstimator->cdfFromFile)
    , cbf(&distEstimator->cbfFromFile)
    , distEstimator(distEstimator)
    , prioCutOffsFromCdf()
    , prioCutOffsFromCbf()
{
    setPrioCutOffs();
}

uint16_t
PriorityResolver::getPrioForPkt(PrioResolutionMode prioMode,
    const uint32_t msgSize, const PktType pktType)
{
    if (prioMode == PrioResolutionMode::FIXED_UNSCHED) {
        if (pktType == PktType::REQUEST || pktType == PktType::UNSCHED_DATA) {
            return 0;
        } else {
            cRuntimeError("Invalid PktType %d for PrioMode %d",
                pktType, prioMode);
        }
    }

    if (prioMode == PrioResolutionMode::FIXED_SCHED) {
        if (pktType == PktType::SCHED_DATA) {
            return numPrios - 1;
        } else {
            cRuntimeError("Invalid PktType %d for PrioMode %d",
                pktType, prioMode);
        }
    }

    std::vector<uint32_t>* cutOffVec = NULL;
    switch (prioMode) {
        case PrioResolutionMode::STATIC_FROM_CDF:
            cutOffVec = &prioCutOffsFromCdf;
            break;
        case PrioResolutionMode::STATIC_FROM_CBF:
            cutOffVec = &prioCutOffsFromCbf;
            break;
        default:
            cRuntimeError("Invalid priority mode: prioMode(%d)", prioMode);
    }

    size_t mid, high, low;
    low = 0;
    high = cutOffVec->size() - 1;
    while(low < high) {
        mid = (high + low) / 2;
        if (msgSize <= cutOffVec->at(mid)) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return (uint16_t)high;
}

void
PriorityResolver::setPrioCutOffs()
{
    ASSERT(cbf->size() == cdf->size() && cdf->at(cdf->size() - 1).second == 1.00
        && cbf->at(cbf->size() - 1).second == 1.00);
    double probMax = 1.0;
    double probStep = probMax/numPrios;
    size_t i = 0;
    size_t j = 0;
    uint32_t prevCutOffCdfSize = 0;
    uint32_t prevCutOffCbfSize = 0;
    for (double prob = probStep; prob < probMax; prob += probStep) {
        for (; i < cdf->size(); i++) {
            if (cdf->at(i).first == prevCutOffCdfSize) {
                // Do not add duplicate sizes to cutOffSizes vector
                continue;
            }
            if (cdf->at(i).second >= prob) {
                prioCutOffsFromCdf.push_back(cdf->at(i).first);
                prevCutOffCdfSize = cdf->at(i).first;
                break;
            }
        }

        for (; j < cbf->size(); j++) {
            if (cbf->at(j).first == prevCutOffCbfSize) {
                continue;
            }
            if (cbf->at(j).second >= prob) {
                prioCutOffsFromCbf.push_back(cbf->at(j).first);
                prevCutOffCbfSize = cbf->at(j).first;
                break;
            }
        }
    }
    prioCutOffsFromCdf.push_back(UINT32_MAX);
    prioCutOffsFromCbf.push_back(UINT32_MAX);
}

PriorityResolver::PrioResolutionMode
PriorityResolver::strPrioModeToInt(const char* prioResMode)
{
    if (strcmp(prioResMode, "STATIC_FROM_CDF") == 0) {
        return PrioResolutionMode::STATIC_FROM_CDF;
    } else if (strcmp(prioResMode, "STATIC_FROM_CBF") == 0) {
        return PrioResolutionMode::STATIC_FROM_CBF;
    } else if (strcmp(prioResMode, "FIXED_UNSCHED") == 0) {
        return PrioResolutionMode::FIXED_UNSCHED;
    } else if (strcmp(prioResMode, "FIXED_SCHED") == 0) {
        return PrioResolutionMode::FIXED_SCHED;
    } else {
        return PrioResolutionMode::INVALID_PRIO_MODE;
    }
}
