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
        SMF_LAST_CAP_CBF,
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
    void setLastCapBytesPrioCutOffs();
    static void printCbfCdf(WorkloadEstimator::CdfVector* vec);
    PrioResolutionMode strPrioModeToInt(const char* prioResMode);

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
    std::vector<uint32_t> prioCutOffsLastCapBytesCbf;
    WorkloadEstimator* distEstimator;
    HomaConfigDepot* homaConfig;

  PROTECTED:
    void recomputeCbf(uint32_t cbfCapMsgSize);
    friend class HomaTransport;
};
#endif /* PRIORITYRESOLVER_H_ */
