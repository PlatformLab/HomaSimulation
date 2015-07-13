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
    ByteBucket(double linkSpeed);
    ~ByteBucket();
    inline simtime_t getGrantTime()
    {
        return nextGrantTime;
    }
    void getGrant(uint32_t grantSize, simtime_t currentTime);
    void subtractUnschedBytes(HomaPkt* reqMsg);

  private:
    double linkSpeed;
    simtime_t nextGrantTime;
};


#endif /* BYTEBUCKET_H_ */
