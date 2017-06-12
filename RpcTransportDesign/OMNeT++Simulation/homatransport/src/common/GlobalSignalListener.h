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

#include <unordered_map>
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
    void handleMesgStatsObj(cObject* obj);

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

    /**
     * This subclass subscribes to end-to-end application stats signal
     * WorkloadSynthesizer::mesgStatsSignal and receives the signal from all
     * instances of WorkloadSynthesizer.  The signal conveys an object that
     * contains end-to-end stats value for different range of message sizes.  It
     * then aggregates the stats values carried by this signal, for diferent
     * ranges of message sizes, across all those workload synthesizer instances.
     * Furthermore, it records the results in a histogram for each range.
     */
    class AppStatsListener : public cListener
    {
      PUBLIC:
        AppStatsListener(GlobalSignalListener* parentMod);
        ~AppStatsListener(){}
        virtual void receiveSignal(cComponent* src, simsignal_t id,
            cObject* obj);
      PUBLIC:
        GlobalSignalListener* parentModule;
    };

  PUBLIC:
    StabilityRecorder* stabilityRecorder;
    AppStatsListener* appStatsListener;

    static simsignal_t mesgBytesOnWireSignal;
    // HashMap from message size range to signals ids. For each range of message
    // size, there is a signal defined to record statistics of that message size
    // range.
    std::unordered_map<uint64_t, simsignal_t> mesgLatencySignals;
    std::unordered_map<uint64_t, simsignal_t> mesgStretchSignals;
    std::unordered_map<uint64_t, simsignal_t> mesgQueueDelaySignals;
    std::unordered_map<uint64_t, simsignal_t> mesgTransportSchedDelaySignals;

  PROTECTED:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
};

#endif
