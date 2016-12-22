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

#include "GlobalSignalListener.h"

Define_Module(GlobalSignalListener);

GlobalSignalListener::GlobalSignalListener()
    : stabilityRecorder(NULL)
{}

GlobalSignalListener::~GlobalSignalListener()
{
    delete stabilityRecorder;
}

void
GlobalSignalListener::initialize()
{
    stabilityRecorder = new StabilityRecorder(this);
}

void
GlobalSignalListener::handleMessage(cMessage *msg)
{}

GlobalSignalListener::StabilityRecorder::StabilityRecorder(
        GlobalSignalListener* parentModule)
    : parentModule(parentModule)
    , totalSendBacklog(0)
    , callCount(0)
    , lastSampleTime(SIMTIME_ZERO)
    , sendBacklog()
{
    // register this object as a listner to stabilitySignal defined in
    // HomaTransport.
    simulation.getSystemModule()->subscribe("stabilitySignal", this);
    sendBacklog.setName("backlogged transmit messages");
}

/**
 * Every time stabilitySignal is emitted, this function is called. It aggregates
 * number of incomplete transmit messages over all transport instances in the
 * simulation and record the total number in the vector result file.
 */
void
GlobalSignalListener::StabilityRecorder::receiveSignal(cComponent* src,
    simsignal_t id, unsigned long l)
{
    simtime_t currentTime = simTime();
    totalSendBacklog += l;
    callCount++;
    if (currentTime != lastSampleTime) {
        // record totalSendBacklog at lastSampleTime to result vector.
        sendBacklog.recordWithTimestamp(lastSampleTime, totalSendBacklog);
        lastSampleTime = currentTime;
        totalSendBacklog = 0;
    }
}
