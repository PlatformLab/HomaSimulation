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

/**
 * This class observes the packets in both receiving and sending path and
 * creates an estimate of the workload characteristics (Average load factor and
 * size distribution.). This class can also receive an explicit workload type
 * as used by the MsgSizeDistributions for comparison purpose.
 */
class WorkloadEstimator
{
  public:
    explicit WorkloadEstimator(const char* workloadType = NULL);
    void recomputeRxWorkload(uint32_t msgSize, simtime_t timeMsgArrived);
    void recomputeSxWorkload(uint32_t msgSize, simtime_t timeMsgSent);

  public:
    typedef std::vector<std::pair<uint32_t, double>> CdfVector;

  public:
    CdfVector cdfFromFile;
    double avgSizeFromFile;
    CdfVector cbfFromFile;
    CdfVector rxCdfComputed;
    CdfVector sxCdfComputed;
    double loadFactor;
};
#endif /* WORKLOADESTIMATOR_H_ */
