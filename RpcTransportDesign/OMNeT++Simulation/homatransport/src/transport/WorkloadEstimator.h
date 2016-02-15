/*
 * WorkloadEstimator.h
 *
 *  Created on: Dec 23, 2015
 *      Author: behnamm
 */

#ifndef WORKLOADESTIMATOR_H_
#define WORKLOADESTIMATOR_H_

#include <vector>
#include <utility>
#include <string>
#include <omnetpp.h>
#include "common/Minimal.h"
#include "transport/HomaConfigDepot.h"

/**
 * This class observes the packets in both receiving and sending path and
 * creates an estimate of the workload characteristics (Average load factor and
 * size distribution.). This class can also receive an explicit workload type
 * as used by the MsgSizeDistributions for comparison purpose.
 */
class WorkloadEstimator
{
  PUBLIC:
    typedef std::vector<std::pair<uint32_t, double>> CdfVector;
    explicit WorkloadEstimator(HomaConfigDepot* homaConfig);
    void recomputeRxWorkload(uint32_t msgSize, simtime_t timeMsgArrived);
    void recomputeSxWorkload(uint32_t msgSize, simtime_t timeMsgSent);

  PUBLIC:
    CdfVector cdfFromFile;
    double avgSizeFromFile;
    CdfVector cbfFromFile;
    CdfVector rxCdfComputed;
    CdfVector sxCdfComputed;
    uint32_t lastCbfCapMsgSize;
    HomaConfigDepot* homaConfig;

  PROTECTED:
    void getCbfFromCdf(CdfVector& cdf, uint32_t cbfCapMsgSize = UINT32_MAX);
    friend class PriorityResolver;

};
#endif /* WORKLOADESTIMATOR_H_ */
