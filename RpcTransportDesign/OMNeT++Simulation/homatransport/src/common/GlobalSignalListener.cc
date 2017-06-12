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

simsignal_t GlobalSignalListener::mesgBytesOnWireSignal =
    registerSignal("mesgBytesOnWire");

GlobalSignalListener::GlobalSignalListener()
    : stabilityRecorder(NULL)
    , appStatsListener(NULL)
    , mesgLatencySignals()
    , mesgStretchSignals()
    , mesgQueueDelaySignals()
    , mesgTransportSchedDelaySignals()
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

void
GlobalSignalListener::handleMesgStatsObj(cObject* obj)
{
    auto* mesgStats = dynamic_cast<MesgStats*>(obj);
    uint64_t mesgSizeBin = mesgStats->mesgSizeBin;

    simtime_t latency = mesgStats->latency;
    double stretch = mesgStats->stretch;
    double queueDelay = mesgStats->queuingDelay;
    simtime_t transptSchedDelay = mesgStats->transportSchedDelay;
    uint64_t mesgSizeOnWire  = mesgStats->mesgSizeOnWire;
    emit(mesgBytesOnWireSignal, mesgSizeOnWire);

    if (mesgLatencySignals.find(mesgSizeBin) == mesgLatencySignals.end()) {
        std::string sizeUpperBound;
        if (mesgSizeBin != UINT64_MAX) {
            sizeUpperBound = std::to_string(mesgSizeBin);
        } else {
            sizeUpperBound = "Huge";
        }

        char latencySignalName[50];
        sprintf(latencySignalName, "mesg%sDelay", sizeUpperBound.c_str());
        simsignal_t latencySignal = registerSignal(latencySignalName);
        mesgLatencySignals[mesgSizeBin] = latencySignal;
        char latencyStatsName[50];
        sprintf(latencyStatsName, "mesg%sDelay", sizeUpperBound.c_str());
        cProperty *statisticTemplate =
            getProperties()->get("statisticTemplate", "mesgDelay");
        ev.addResultRecorders(this, latencySignal, latencyStatsName,
            statisticTemplate);

        char stretchSignalName[50];
        sprintf(stretchSignalName, "mesg%sStretch", sizeUpperBound.c_str());
        simsignal_t stretchSignal = registerSignal(stretchSignalName);
        mesgStretchSignals[mesgSizeBin] = stretchSignal;
        char stretchStatsName[50];
        sprintf(stretchStatsName, "mesg%sStretch", sizeUpperBound.c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
            "mesgStretch");
        ev.addResultRecorders(this, stretchSignal, stretchStatsName,
            statisticTemplate);

        char queueDelaySignalName[50];
        sprintf(queueDelaySignalName, "mesg%sQueueDelay",
                sizeUpperBound.c_str());
        simsignal_t queueDelaySignal = registerSignal(queueDelaySignalName);
        mesgQueueDelaySignals[mesgSizeBin] = queueDelaySignal;
        char queueDelayStatsName[50];
        sprintf(queueDelayStatsName, "mesg%sQueueDelay",
                sizeUpperBound.c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "mesgQueueDelay");
        ev.addResultRecorders(this, queueDelaySignal, queueDelayStatsName,
                statisticTemplate);

        char transportSchedDelaySignalName[50];
        sprintf(transportSchedDelaySignalName, "mesg%sTransportSchedDelay",
            sizeUpperBound.c_str());
        simsignal_t transportSchedDelaySignal =
            registerSignal(transportSchedDelaySignalName);
        mesgTransportSchedDelaySignals[mesgSizeBin] =
            transportSchedDelaySignal;
        char transportSchedDelayStatsName[50];
        sprintf(transportSchedDelayStatsName, "mesg%sTransportSchedDelay",
            sizeUpperBound.c_str());
        statisticTemplate = getProperties()->get("statisticTemplate",
                "mesgTransportSchedDelay");
        ev.addResultRecorders(this, transportSchedDelaySignal,
            transportSchedDelayStatsName, statisticTemplate);
    }

    emit(mesgLatencySignals.at(mesgSizeBin), latency);
    emit(mesgStretchSignals.at(mesgSizeBin), stretch);
    emit(mesgQueueDelaySignals.at(mesgSizeBin), queueDelay);
    emit(mesgTransportSchedDelaySignals.at(mesgSizeBin), transptSchedDelay);
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
//  auto* mesgStats = dynamic_cast<MesgStats*>(obj);
//  auto* srcMod = dynamic_cast<cModule*>(src);
//  std::cout << "Received MesgStats signal from: " << srcMod->getFullPath() <<
//      ", mesg size: " << mesgStats->mesgSize << ", SizeOnWire: " <<
//      mesgStats->mesgSizeOnWire << ", size bin: " << mesgStats->mesgSizeBin <<
//      ", latency: " << mesgStats->latency << ", stretch: " <<
//      mesgStats->stretch << ", queueDelay: " << mesgStats->queuingDelay <<
//      ", transportSchedDelay: " << mesgStats->transportSchedDelay << std::endl;
    parentModule->handleMesgStatsObj(obj);
}
