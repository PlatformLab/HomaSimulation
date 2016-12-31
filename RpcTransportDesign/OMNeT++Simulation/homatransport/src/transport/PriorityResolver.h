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
        STATIC_FROM_CDF = 0,
        STATIC_FROM_CBF,
        STATIC_EXP_CDF,
        STATIC_EXP_CBF,
        FIXED_UNSCHED,
        FIXED_SCHED,
        SIMULATED_SRBF,
        SMF_CBF_BASED,
        HEAD_TAIL_BYTES_FIRST_EQUAL_BYTES,
        HEAD_TAIL_BYTES_FIRST_EQUAL_COUNTS,
        HEAD_TAIL_BYTES_FIRST_EXP_BYTES,
        HEAD_TAIL_BYTES_FIRST_EXP_COUNTS,
        INVALID_PRIO_MODE        // Always the last value
    };
    explicit PriorityResolver(HomaConfigDepot* homaConfig,
        WorkloadEstimator* distEstimator);
    std::vector<uint16_t> getUnschedPktsPrio(PrioResolutionMode prioMode,
        const OutboundMessage* outbndMsg);
    uint16_t getSchedPktPrio(PrioResolutionMode prioMode,
        const InboundMessage* inbndMsg);
    void setCdfPrioCutOffs();
    void setCbfPrioCutOffs();
    void setExpFromCdfPrioCutOffs();
    void setExpFromCbfPrioCutOffs();
    void setHeadTailFirstPrioCutOffs();
    void setExpHeadTailFirstPrioCutOffs();
    static void printCbfCdf(WorkloadEstimator::CdfVector* vec);
    PrioResolutionMode strPrioModeToInt(const char* prioResMode);

    // Used for comparing double values. return true if a smaller than b, within
    // an epsilon bound.
    inline bool isLess(double a, double b)
    {
            return a+1e-6 < b;
    }

  PRIVATE:
    uint32_t maxSchedPktDataBytes;
    uint32_t lastCbfCapMsgSize;
    const WorkloadEstimator::CdfVector* cdf;
    const WorkloadEstimator::CdfVector* cbf;
    const WorkloadEstimator::CdfVector* cbfLastCapBytes;
    std::vector<uint32_t> prioCutOffsFromCdf;
    std::vector<uint32_t> prioCutOffsFromCbf;
    std::vector<uint32_t> prioCutOffsExpCbf;
    std::vector<uint32_t> prioCutOffsExpCdf;
    std::vector<uint32_t> prioCutOffsHeadTailFirst;
    std::vector<uint32_t> prioCutOffsHeadTailFirstExp;
    WorkloadEstimator* distEstimator;
    HomaConfigDepot* homaConfig;

  PROTECTED:
    void recomputeCbf(uint32_t cbfCapMsgSize);
    friend class HomaTransport;
};
#endif /* PRIORITYRESOLVER_H_ */
