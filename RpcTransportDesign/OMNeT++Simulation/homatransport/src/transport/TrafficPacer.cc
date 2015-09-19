/*
 * TrafficPacer.cc
 *
 *  Created on: Sep 18, 2015
 *      Author: neverhood
 */

#include <omnetpp.h>
#include "TrafficPacer.h"

TrafficPacer::TrafficPacer(double nominalLinkSpeed,
        uint32_t maxAllowedInFlightBytes)
    : actualLinkSpeed(nominalLinkSpeed * ACTUAL_TO_NOMINAL_RATE_RATIO)
    , nextGrantTime(SIMTIME_ZERO)
    , totalOutstandingBytes(0)
    , maxAllowedInFlightBytes(maxAllowedInFlightBytes)
{}

TrafficPacer::~TrafficPacer()
{}


/**
 * returns a lower bound on the next grant time
 */
simtime_t
TrafficPacer::grantSent(uint32_t grantedPktSizeOnWire, simtime_t currentTime)
{
    nextGrantTime = SimTime(1e-9 * 
            (grantedPktSizeOnWire * 8.0 / actualLinkSpeed)) + currentTime;
    totalOutstandingBytes += grantedPktSizeOnWire;
    return nextGrantTime;
}

bool
TrafficPacer::okToGrant(simtime_t currentTime)
{
    return (nextGrantTime <= currentTime) && 
            (totalOutstandingBytes < (int)maxAllowedInFlightBytes);
}

void
TrafficPacer::bytesArrived(uint32_t arrivedBytes)
{
    totalOutstandingBytes -= arrivedBytes;
}

void
TrafficPacer::unschedPendingBytes(uint32_t committedBytes)
{
    totalOutstandingBytes += committedBytes;
}
