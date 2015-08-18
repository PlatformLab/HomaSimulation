/*
 * ByteBucket.cc
 *
 *  Created on: Jul 1, 2015
 *      Author: neverhood
 */

#include <omnetpp.h>
#include "ByteBucket.h"

ByteBucket::ByteBucket(double nominalLinkSpeed)
    : actualLinkSpeed(nominalLinkSpeed * ACTUAL_TO_NOMINAL_RATE_RATIO)
    , nextGrantTime(SIMTIME_ZERO)
{}

ByteBucket::~ByteBucket()
{}

void
ByteBucket::getGrant(uint32_t grantSize, simtime_t currentTime)
{
    nextGrantTime = SimTime(1e-9 * (grantSize * 8.0 / actualLinkSpeed)) + currentTime;
    return;
}

void
ByteBucket::subtractUnschedBytes(HomaPkt* reqMsg)
{}
