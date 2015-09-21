/*
 * TrafficPacer.h
 *
 *  Created on: Sep 18, 2015
 *      Author: neverhood
 */

#ifndef TRAFFICPACER_H_
#define TRAFFICPACER_H_

#include <omnetpp.h>
#include "transport/HomaPkt.h"

class TrafficPacer
{
  public:
    TrafficPacer(double nominalLinkSpeed,
            uint32_t maxAllowedInFlightBytes = 15000);
    ~TrafficPacer();
    static constexpr double ACTUAL_TO_NOMINAL_RATE_RATIO = 1.0;

    simtime_t grantSent(uint32_t grantedPktSizeOnWire, simtime_t currentTime);
    void bytesArrived(uint32_t arrivedBytes);
    void unschedPendingBytes(uint32_t committedBytes);
    bool okToGrant(simtime_t currentTime);

  private:
    double actualLinkSpeed;
    simtime_t nextGrantTime;
    int totalOutstandingBytes;

    uint32_t maxAllowedInFlightBytes;
};


#endif /* TRAFFICPACER_H_ */
