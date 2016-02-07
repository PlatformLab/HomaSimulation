/*
 * PriorityResolver.cc
 *
 *  Created on: Dec 23, 2015
 *      Author: behnamm
 */

#include "PriorityResolver.h"

PriorityResolver::PriorityResolver(const uint32_t numPrios,
        WorkloadEstimator* distEstimator)
    : numPrios(numPrios)
    , lastCbfCapMsgSize(UINT32_MAX)
    , cdf(&distEstimator->cdfFromFile)
    , cbf(&distEstimator->cbfFromFile)
    , distEstimator(distEstimator)
    , prioCutOffsFromCdf()
    , prioCutOffsFromCbf()
    , prioCutOffsExpCbf()
    , prioCutOffsExpCdf()
{
    setCdfPrioCutOffs();
    setCbfPrioCutOffs();
    setExpFromCdfPrioCutOffs();
    setExpFromCbfPrioCutOffs();
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
        case PrioResolutionMode::STATIC_EXP_CDF:
            cutOffVec = &prioCutOffsExpCdf;
            break;
        case PrioResolutionMode::STATIC_EXP_CBF:
            cutOffVec = &prioCutOffsExpCbf;
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
PriorityResolver::recomputeCbf(uint32_t cbfCapMsgSize)
{
    if (cbfCapMsgSize != lastCbfCapMsgSize) {
        distEstimator->getCbfFromCdf(distEstimator->cdfFromFile,
            cbfCapMsgSize);
        setCbfPrioCutOffs();
        setExpFromCbfPrioCutOffs();
        lastCbfCapMsgSize = cbfCapMsgSize;
    }
}

void
PriorityResolver::setCdfPrioCutOffs()
{
    prioCutOffsFromCdf.clear();
    ASSERT(cbf->size() == cdf->size() && cdf->at(cdf->size() - 1).second == 1.00
        && cbf->at(cbf->size() - 1).second == 1.00);
    double probMax = 1.0;
    double probStep = probMax/numPrios;
    size_t i = 0;
    uint32_t prevCutOffCdfSize = UINT32_MAX;
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

    }
    prioCutOffsFromCdf.push_back(UINT32_MAX);
}

void
PriorityResolver::setExpFromCdfPrioCutOffs()
{
    prioCutOffsExpCdf.clear();
    ASSERT(cbf->size() == cdf->size() && cdf->at(cdf->size() - 1).second == 1.00
        && cbf->at(cbf->size() - 1).second == 1.00);
    double probMax = 1.0;
    double probStep = probMax / (2 - pow(2.0, 1-(int)(numPrios)));
    size_t i = 0;
    uint32_t prevCutOffExpCdfSize = UINT32_MAX;
    for (double prob = probStep; prob < probMax; prob += probStep) {
        probStep /= 2.0;
        for (; i < cdf->size(); i++) {
            if (cdf->at(i).first == prevCutOffExpCdfSize) {
                // Do not add duplicate sizes to cutOffSizes vector
                continue;
            }
            if (cdf->at(i).second >= prob) {
                prioCutOffsExpCdf.push_back(cdf->at(i).first);
                prevCutOffExpCdfSize = cdf->at(i).first;
                break;
            }
        }
    }
    prioCutOffsExpCdf.push_back(UINT32_MAX);
}

void
PriorityResolver::setCbfPrioCutOffs()
{
    prioCutOffsFromCbf.clear();
    ASSERT(cbf->size() == cdf->size() && cdf->at(cdf->size() - 1).second == 1.00
        && cbf->at(cbf->size() - 1).second == 1.00);
    double probMax = 1.0;
    double probStep = probMax/numPrios;
    size_t j = 0;
    uint32_t prevCutOffCbfSize = UINT32_MAX;
    for (double prob = probStep; prob < probMax; prob += probStep) {
        for (; j < cbf->size(); j++) {
            if (cbf->at(j).first == prevCutOffCbfSize) {
                // Do not add duplicate sizes to cutOffSizes vector
                continue;
            }
            if (cbf->at(j).second >= prob) {
                prioCutOffsFromCbf.push_back(cbf->at(j).first);
                prevCutOffCbfSize = cbf->at(j).first;
                break;
            }
        }
    }
    prioCutOffsFromCbf.push_back(UINT32_MAX);
}

void
PriorityResolver::setExpFromCbfPrioCutOffs()
{
    prioCutOffsExpCbf.clear();
    ASSERT(cbf->size() == cdf->size() && cdf->at(cdf->size() - 1).second == 1.00
        && cbf->at(cbf->size() - 1).second == 1.00);
    double probMax = 1.0;
    double probStep = probMax / (2 - pow(2.0, 1-(int)(numPrios)));
    size_t i = 0;
    uint32_t prevCutOffExpCbfSize = UINT32_MAX;
    for (double prob = probStep; prob < probMax; prob += probStep) {
        probStep /= 2.0;
        for (; i < cbf->size(); i++) {
            if (cbf->at(i).first == prevCutOffExpCbfSize) {
                // Do not add duplicate sizes to cutOffSizes vector
                continue;
            }
            if (cbf->at(i).second >= prob) {
                prioCutOffsExpCbf.push_back(cbf->at(i).first);
                prevCutOffExpCbfSize = cbf->at(i).first;
                break;
            }
        }
    }
    prioCutOffsExpCbf.push_back(UINT32_MAX);

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
    } else if (strcmp(prioResMode, "STATIC_EXP_CDF") == 0) {
        return PrioResolutionMode::STATIC_EXP_CDF;
    } else if (strcmp(prioResMode, "STATIC_EXP_CBF") == 0) {
        return PrioResolutionMode::STATIC_EXP_CBF;
    } else {
        return PrioResolutionMode::INVALID_PRIO_MODE;
    }
}

void
PriorityResolver::printCbfCdf(WorkloadEstimator::CdfVector* vec)
{
    for (auto elem:*vec) {
        std::cout << elem.first << " : " << elem.second << std::endl;
    }
}
