//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

#ifndef __HOMATRANSPORT_GLOBAL_SIGNAL_LISTENER_H_
#define __HOMATRANSPORT_GLOBAL_SIGNAL_LISTENER_H_

#include <omnetpp.h>
#include "common/Minimal.h"

/**
 * This module is intended as place at the top simulation level that collects
 * signals and record statistics over the whole simulation. For example, you can
 * define a new listener class in this module that receives signals from all
 * transport modules and aggregate some statistic over those signals and and
 * record the statistics in vector result files.
 */
class GlobalSignalListener : public cSimpleModule
{
  PUBLIC:
    GlobalSignalListener();
    ~GlobalSignalListener();

    /**
     * This subclass subscribes to HomaTransport::stabilitySignal and receives
     * this signal from all instances of the HomaTransport. It aggregates total
     * number of messages to be transmitted in all of the instances and records
     * the result in a vector.
     */
    class StabilityRecorder : public cListener
    {
      PUBLIC:
        StabilityRecorder(GlobalSignalListener* parentModule);
        ~StabilityRecorder(){}
        virtual void receiveSignal(cComponent* src, simsignal_t id,
            unsigned long l);

      PUBLIC:
        GlobalSignalListener* parentModule;
        uint64_t totalSendBacklog;
        uint64_t callCount;
        simtime_t lastSampleTime;
        cOutVector sendBacklog;
        
    };

    StabilityRecorder* stabilityRecorder;

  PROTECTED:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);

};

#endif
