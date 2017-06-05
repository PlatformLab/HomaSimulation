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
#include "WorkloadSynthesizer.h"

Define_Module(GlobalSignalListener);

GlobalSignalListener::GlobalSignalListener()
    : stabilityRecorder(NULL)
    , appStatsListener(NULL)
{}

GlobalSignalListener::~GlobalSignalListener()
{
    delete stabilityRecorder;
    delete appStatsListener;
}

void
GlobalSignalListener::initialize()
{
    stabilityRecorder = new StabilityRecorder(this);
    appStatsListener = new AppStatsListener(this);

}

void
GlobalSignalListener::handleMessage(cMessage *msg)
{
    throw cRuntimeError("GlobalSignalListener shouldn't receive any message!");
}

GlobalSignalListener::StabilityRecorder::StabilityRecorder(
        GlobalSignalListener* parentModule)
    : parentModule(parentModule)
    , totalSendBacklog(0)
    , callCount(0)
    , lastSampleTime(SIMTIME_ZERO)
    , sendBacklog()
{
    // Register this object as a listner to stabilitySignal defined in
    // HomaTransport.
    simulation.getSystemModule()->subscribe("stabilitySignal", this);
    sendBacklog.setName("backlogged transmit messages");
}

/**
 * Every time stabilitySignal is emitted, this function is called. It aggregates
 * number of the incomplete transmit messages over all transport instances in
 * the simulation and records the total number in the vector result file.
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

/**
 *
 */
GlobalSignalListener::AppStatsListener::AppStatsListener(
        GlobalSignalListener* parentMod)
    : parentModule(parentMod)
{
    // Register this object as a listener to the "msgStats" singal
    // WorkloadSynthesizer fire. This signal contains the end-to-end stats
    // collected at "WorkloadSynthesizer" instances.
    simulation.getSystemModule()->subscribe("mesgStats", this);
}

/**
 *
 */
void
GlobalSignalListener::AppStatsListener::receiveSignal(cComponent* src,
    simsignal_t id, cObject* obj)
{
    auto* mesgStats = dynamic_cast<MesgStats*>(obj);
    auto* srcMod = dynamic_cast<cModule*>(src);

    std::cout << "Received MesgStats signal from: " << srcMod->getFullPath() <<
        ", mesg size: " << mesgStats->mesgSize << ", SizeOnWire: " <<
        mesgStats->mesgSizeOnWire << ", size bin: " << mesgStats->mesgSizeBin <<
        ", latency: " << mesgStats->latency << ", stretch: " <<
        mesgStats->stretch << ", queueDelay: " << mesgStats->queuingDelay <<
        ", transportSchedDelay: " << mesgStats->transportSchedDelay << std::endl;


 }
