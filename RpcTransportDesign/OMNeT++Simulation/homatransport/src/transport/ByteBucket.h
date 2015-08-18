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
    ByteBucket(double nominalLinkSpeed);
    ~ByteBucket();
    static constexpr double ACTUAL_TO_NOMINAL_RATE_RATIO = 0.95;

    inline simtime_t getGrantTime()
    {
        return nextGrantTime;
    }
    void getGrant(uint32_t grantSize, simtime_t currentTime);
    void subtractUnschedBytes(HomaPkt* reqMsg);

  private:
    double actualLinkSpeed;
    simtime_t nextGrantTime;
};


#endif /* BYTEBUCKET_H_ */
