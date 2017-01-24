/*
 * PriorityResolver.h
 *
 *  Created on: Dec 23, 2015
 *      Author: behnamm
 */

#ifndef PRIORITYRESOLVER_H_
#define PRIORITYRESOLVER_H_

#include "common/Minimal.h"
#include "transport/WorkloadEstimator.h"
#include "transport/HomaPkt.h"
#include "transport/HomaConfigDepot.h"
#include "transport/HomaTransport.h"

class PriorityResolver
{
  PUBLIC:
    typedef HomaTransport::InboundMessage InboundMessage;
    typedef HomaTransport::OutboundMessage OutboundMessage;
    enum PrioResolutionMode {
        STATIC_CDF_UNIFORM,
        STATIC_CBF_UNIFORM,
        STATIC_CBF_GRADUATED,
        EXPLICIT,
        FIXED_UNSCHED,
        INVALID_PRIO_MODE        // Always the last value
    };

    explicit PriorityResolver(HomaConfigDepot* homaConfig,
        WorkloadEstimator* distEstimator);
    std::vector<uint16_t> getUnschedPktsPrio(const OutboundMessage* outbndMsg);
    uint16_t getSchedPktPrio(const InboundMessage* inbndMsg);
    void setPrioCutOffs();
    static void printCbfCdf(WorkloadEstimator::CdfVector* vec);
    PrioResolutionMode strPrioModeToInt(const char* prioResMode);

    // Used for comparing double values. returns true if a smaller than b,
    // within an epsilon bound.
    inline bool isLess(double a, double b)
    {
            return a+1e-6 < b;
    }

    // Used for comparing double values. returns true if a and b are only within
    // some epsilon value from each other.
    inline bool isEqual(double a, double b)
    {
        return fabs(a-b) < 1e-6;
    }

  PRIVATE:
    uint32_t maxSchedPktDataBytes;
    const WorkloadEstimator::CdfVector* cdf;
    const WorkloadEstimator::CdfVector* cbf;
    const WorkloadEstimator::CdfVector* cbfLastCapBytes;
    const WorkloadEstimator::CdfVector* remainSizeCbf;
    std::vector<uint32_t> prioCutOffs;
    WorkloadEstimator* distEstimator;
    PrioResolutionMode prioResMode;
    HomaConfigDepot* homaConfig;

  PROTECTED:
    void recomputeCbf(uint32_t cbfCapMsgSize, uint32_t boostTailBytesPrio);
    uint16_t getMesgPrio(uint32_t size);
    friend class HomaTransport;
};
#endif /* PRIORITYRESOLVER_H_ */
