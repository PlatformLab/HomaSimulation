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
    CdfVector cbfFromFile;
    CdfVector cbfLastCapBytesFromFile;
    CdfVector rxCdfComputed;
    CdfVector sxCdfComputed;
    double avgSizeFromFile;
    uint32_t lastCbfCapMsgSize;
    HomaConfigDepot* homaConfig;

  PROTECTED:
    void getCbfFromCdf(CdfVector& cdf, uint32_t cbfCapMsgSize = UINT32_MAX);
    class CompCdfPairs {
      PUBLIC:
        CompCdfPairs() {}
        bool operator()(const std::pair<uint32_t, double>& pair1,
                const std::pair<uint32_t, double>& pair2)
        {
            return pair1.first < pair2.first;
        }
    };
    friend class PriorityResolver;

};
#endif /* WORKLOADESTIMATOR_H_ */
