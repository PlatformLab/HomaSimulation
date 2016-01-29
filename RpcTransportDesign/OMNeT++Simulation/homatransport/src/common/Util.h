/*
 * Util.h
 *
 *  Created on: Jan 27, 2016
 *      Author: behnamm
 */

#ifndef UTIL_H_
#define UTIL_H_

#include <omnetpp.h>

class SIM_API HomaMsgSizeFilter : public cObjectResultFilter
{
  public:
    HomaMsgSizeFilter() {}
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t,
        cObject *object);
};

class SIM_API HomaPktBytesFilter : public cObjectResultFilter
{
  public:
    HomaPktBytesFilter() {}
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t,
        cObject *object);
};

class SIM_API HomaUnschedPktBytesFilter : public cObjectResultFilter
{
  public:
    HomaUnschedPktBytesFilter() {}
    virtual void receiveSignal(cResultFilter *prev, simtime_t_cref t,
        cObject *object);
};

#endif /* UTIL_H_ */
