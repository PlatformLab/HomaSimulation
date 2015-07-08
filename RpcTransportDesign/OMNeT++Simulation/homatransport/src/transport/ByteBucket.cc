/*
 * ByteBucket.cc
 *
 *  Created on: Jul 1, 2015
 *      Author: neverhood
 */

#include <omnetpp.h>
#include "ByteBucket.h"

ByteBucket::ByteBucket(uint32_t linkSpeed, uint32_t maxRtt)
    : linkSpeed(linkSpeed)
    , maxRtt(maxRtt)
    , bucketMaxBytes(0)
    , availableBytes(0)
    , lastGrantTime(0)
{
    // maxRtt in micro seconds and linkSpeed in Gb/s
    bucketMaxBytes = linkSpeed * maxRtt * 1e3 / 8;
}

ByteBucket::~ByteBucket()
{}

uint32_t
ByteBucket::getGrantBytes(uint32_t numRequestBytes,
        simtime_t currentTime)
{
    if (currentTime < lastGrantTime) {
        cRuntimeError("currentTime(%lu ns) smaller than lastGrantTime(%lu ns)",
                currentTime.inUnit(SIMTIME_NS),
                lastGrantTime.inUnit(SIMTIME_NS)); 
    }

    availableBytes += 
            (currentTime.inUnit(SIMTIME_NS) - lastGrantTime.inUnit(SIMTIME_NS))
            * linkSpeed / 8;

    lastGrantTime = currentTime;
    availableBytes = (availableBytes > (int)bucketMaxBytes) ?
            bucketMaxBytes : availableBytes;

    if (availableBytes > (int)numRequestBytes) {
        availableBytes -= numRequestBytes;
        return numRequestBytes;
    } else if (availableBytes > 0) {
        uint32_t bytesToGrant = availableBytes;
        availableBytes -= availableBytes;
        return bytesToGrant;
    } else {
        return availableBytes;
    }
}

void
ByteBucket::subtractUnschedBytes(HomaPkt* reqMsg)
{}


