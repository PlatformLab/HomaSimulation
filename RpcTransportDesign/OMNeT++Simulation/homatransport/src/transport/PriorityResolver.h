/*
 * PriorityResolver.h
 *
 *  Created on: Dec 23, 2015
 *      Author: behnamm
 */

#ifndef PRIORITYRESOLVER_H_
#define PRIORITYRESOLVER_H_

#include <transport/WorkloadEstimator.h>
#include <transport/HomaPkt.h>

class PriorityResolver
{
  public:
    enum PrioResolutionMode {
        STATIC_FROM_CDF = 0,
        STATIC_FROM_CBF,
        STATIC_EXP_CDF,
        STATIC_EXP_CBF,
        FIXED_UNSCHED,
        FIXED_SCHED,
        INVALID_PRIO_MODE        // Always the last value
    };
    explicit PriorityResolver(const uint32_t numPrios,
        WorkloadEstimator* distEstimator);
    uint16_t getPrioForPkt(PrioResolutionMode prioMode, const uint32_t msgSize,
        const PktType pktType);
    void setCdfPrioCutOffs();
    void setCbfPrioCutOffs();
    void setExpFromCdfPrioCutOffs();
    void setExpFromCbfPrioCutOffs();
    static void printCbfCdf(WorkloadEstimator::CdfVector* vec);
    PrioResolutionMode strPrioModeToInt(const char* prioResMode);

  private:
    uint32_t numPrios;
    uint32_t lastCbfCapMsgSize;
    const WorkloadEstimator::CdfVector* cdf;
    const WorkloadEstimator::CdfVector* cbf;
    WorkloadEstimator* distEstimator;
    std::vector<uint32_t> prioCutOffsFromCdf;
    std::vector<uint32_t> prioCutOffsFromCbf;
    std::vector<uint32_t> prioCutOffsExpCbf;
    std::vector<uint32_t> prioCutOffsExpCdf;

  protected:
    void recomputeCbf(uint32_t cbfCapMsgSize);
    friend class HomaTransport;
};
#endif /* PRIORITYRESOLVER_H_ */
