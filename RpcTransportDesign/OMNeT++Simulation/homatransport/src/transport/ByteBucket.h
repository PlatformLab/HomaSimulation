/*
 * ByteBucket.h
 *
 *  Created on: Jul 1, 2015
 *      Author: neverhood
 */

#ifndef BYTEBUCKET_H_
#define BYTEBUCKET_H_

#include <omnetpp.h>
#include "transport/HomaPkt_m.h"

class ByteBucket
{
  public:
    ByteBucket(uint32_t linkSpeed, uint32_t maxRtt);
    ~ByteBucket();
    uint32_t getGrantBytes(uint32_t numRequestBytes, simtime_t currentTime);
    void subtractUnschedBytes(HomaPkt* reqMsg);

  private:
    uint32_t linkSpeed;
    uint32_t maxRtt;
    uint32_t bucketMaxBytes;
    int availableBytes;
    simtime_t lastGrantTime;
};


#endif /* BYTEBUCKET_H_ */
