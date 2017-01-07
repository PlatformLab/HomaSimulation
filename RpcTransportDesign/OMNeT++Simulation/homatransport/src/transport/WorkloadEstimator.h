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
    typedef std::list<std::pair<uint32_t, double>> CdfList;
    explicit WorkloadEstimator(HomaConfigDepot* homaConfig);
    void recomputeRxWorkload(uint32_t msgSize, simtime_t timeMsgArrived);
    void recomputeSxWorkload(uint32_t msgSize, simtime_t timeMsgSent);

  PUBLIC:
    CdfVector cdfFromFile; // This has no practical use or meaning
    CdfVector cbfFromFile;
    CdfVector remainSizeCdf;
    CdfVector remainSizeCbf;
    CdfVector cbfLastCapBytesFromFile;
    double avgSizeFromFile;

    CdfVector rxCdfComputed;
    CdfVector sxCdfComputed;
    HomaConfigDepot* homaConfig;

  PROTECTED:
    void getRemainSizeCdfCbf(CdfVector& cdf,
        uint32_t cbfCapMsgSize = UINT32_MAX, uint32_t boostTailBytesPrio = 0);

    void getCbfFromCdf(CdfVector& cdf,
        uint32_t cbfCapMsgSize = UINT32_MAX, uint32_t boostTailBytesPrio = 0);

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
