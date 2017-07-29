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

#include <algorithm>
#include "GlobalSignalListener.h"
#include "application/WorkloadSynthesizer.h"
#include "transport/HomaTransport.h"

Define_Module(GlobalSignalListener);

simsignal_t GlobalSignalListener::bytesOnWireSignal =
    registerSignal("bytesOnWire");

GlobalSignalListener::GlobalSignalListener()
    : stabilityRecorder(NULL)
    , appStatsListener(NULL)
    , activeSchedsListener(NULL)
    , selfWastedBwListeners()
    , mesgBytesOnWireSignals()
    , mesgLatencySignals()
    , mesgStretchSignals()
    , mesgQueueDelaySignals()
    , mesgTransportSchedDelaySignals()
{}

GlobalSignalListener::~GlobalSignalListener()
{
    delete stabilityRecorder;
    delete appStatsListener;
    delete activeSchedsListener;
    for (auto iter : selfWastedBwListeners) {
        delete iter;
    }
}

void
GlobalSignalListener::initialize()
{
    stabilityRecorder = new StabilityRecorder(this);
    appStatsListener = new AppStatsListener(this);
    activeSchedsListener = new ActiveSchedsListener(this);
    selfWastedBwListeners.push_back(new SelfWastedBandwidthListener(this,
      "higherRxSelfWasteTime", "highrSelfBwWaste"));
    selfWastedBwListeners.push_back(new SelfWastedBandwidthListener(this,
      "lowerRxSelfWasteTime", "lowerSelfBwWaste"));
}

void
GlobalSignalListener::finish()
{
    activeSchedsListener->dumpStats();
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
    emit(bytesOnWireSignal, mesgSizeOnWire);

    if (mesgLatencySignals.find(mesgSizeBin) == mesgLatencySignals.end()) {
        std::string sizeUpperBound;
        if (mesgSizeBin != UINT64_MAX) {
            sizeUpperBound = std::to_string(mesgSizeBin);
        } else {
            sizeUpperBound = "Huge";
        }

        char bytesOnWireSigName[50];
        sprintf(bytesOnWireSigName,
            "mesg%sBytesOnWire", sizeUpperBound.c_str());
        simsignal_t bytesOnWireSig = registerSignal(bytesOnWireSigName);
        mesgBytesOnWireSignals[mesgSizeBin] = bytesOnWireSig;
        char bytesOnWireStatsName[50];
        sprintf(bytesOnWireStatsName, "mesg%sBytesOnWire",
            sizeUpperBound.c_str());
        cProperty *statisticTemplate =
            getProperties()->get("statisticTemplate", "mesgBytesOnWire");
        ev.addResultRecorders(this, bytesOnWireSig, bytesOnWireStatsName,
            statisticTemplate);

        char latencySignalName[50];
        sprintf(latencySignalName, "mesg%sDelay", sizeUpperBound.c_str());
        simsignal_t latencySignal = registerSignal(latencySignalName);
        mesgLatencySignals[mesgSizeBin] = latencySignal;
        char latencyStatsName[50];
        sprintf(latencyStatsName, "mesg%sDelay", sizeUpperBound.c_str());
        statisticTemplate =
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

    emit(mesgBytesOnWireSignals.at(mesgSizeBin), mesgSizeOnWire);
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
 * Listener for the WorkloadSynthesizer::mesgStatsSignal. This listener collect
 * the end-to-end message stats carried in the signal and record the aggregated
 * stats for all instances of WorkloadSynthesizer.
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
 * Signal receiver implementation.
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
//      mesgStats->stretch << ", queueDelay: " <<
//      mesgStats->queuingDelay << //      ", transportSchedDelay: " <<
//      mesgStats->transportSchedDelay << std::endl;
    parentModule->handleMesgStatsObj(obj);
}

/**
 * This listener receives signals of type HomaTransport::activeSchedsSignal
 * which carries time
 */
GlobalSignalListener::ActiveSchedsListener::ActiveSchedsListener(
        GlobalSignalListener* parentMod)
    : parentMod(parentMod)
    , activeSxTimes()
    , activeSenders()
    , srcComponents()
    , numEmitterTransport(0)
{
    // Register this object as a listener to the "activeScheds" singal that
    // HomaTransport fires. This signal carries a pointer to an object of type
    // HomaTransport::ReceiveScheduler::ActiveScheds.
    simulation.getSystemModule()->subscribe("activeScheds", this);
    activeSenders.setName("Time division of num active sched senders");
}

/**
 * Signal receiver implementation
 */
void
GlobalSignalListener::ActiveSchedsListener::receiveSignal(cComponent* src,
        simsignal_t id, cObject* obj)
{
    // record time division stats of number of active scheduled in the
    // activeSxTimes map
    auto activeSxObj =
        dynamic_cast<ActiveScheds*>(obj);
    uint32_t numActiveSenders = activeSxObj->numActiveSenders;
    simtime_t duration = activeSxObj->duration;
    auto search = activeSxTimes.find(numActiveSenders);
    if (search != activeSxTimes.end()) {
        search->second += duration.dbl();
    } else {
        activeSxTimes[numActiveSenders] = duration.dbl();
    }

    // check if the emitter source of the signal is transport instance we
    // haven't heard from before. If yes, increment the numEmitterTransport.
    auto ret = srcComponents.insert(src);
    if (ret.second) {
        // The src wasn't in the set and is inserted now
        numEmitterTransport++;
    }
}

/**
 * Write the recorded numActiveSenders and their time durations into the output
 * vector "activeSenders". The numActiveSenders are asceingly sorted and the
 * time duration for each numActiveSenders value is cumulative time durations
 * correspondingly to all smaller numActiveSenders values.
 */
void
GlobalSignalListener::ActiveSchedsListener::dumpStats()
{
    // Read the hash table in a vector of key-value pairs and sort the vector
    // by the key.
    std::vector<std::pair<uint32_t, double>> activeCumTimes(
        activeSxTimes.begin(), activeSxTimes.end());
    std::sort(activeCumTimes.begin(), activeCumTimes.end(),
        std::less<std::pair<uint32_t, double>>());

    // iterate over all recorded numActiveSenders and the cumulative time
    // durations and dmup them in the vector output.
    double cumTimes = 0;
    for (auto it = activeCumTimes.begin(); it != activeCumTimes.end(); it++) {
        cumTimes += (it->second / numEmitterTransport);
        activeSenders.recordWithTimestamp(cumTimes, it->first);
    }
}

/**
 * Constructor for SelfWastedBandwidthListener.
 * \param parentMod
 *      pointer to the GlobalSignalListener instance that owns this listener
 * \param srcSigName
 *      name of one of the HomaTransport::lowerSelfWasteSignal or
 *      HomaTransport::highrSelfWasteSignal that this listener receives.
 * \param destSigName
 *      signal name of the corresponding lowerSelfWasteSignal or
 *      highrSelfWasteSignal that this listener fires value for, every time it
 *      receives signal from srcSigName.
 */
GlobalSignalListener::SelfWastedBandwidthListener::SelfWastedBandwidthListener(
        GlobalSignalListener* parentMod, const char* srcSigName,
        const char* destSigName)
    : parentMod(parentMod)
    , srcSigName(srcSigName)
    , destSigName(destSigName)
    , destSigId(0)

{
    simulation.getSystemModule()->subscribe(srcSigName, this);
    destSigId = registerSignal(destSigName);
}

/**
 * Signal receiver implementation for self inflicted bandwidht wastage.
 * Receives the signal and
 */
void
GlobalSignalListener::SelfWastedBandwidthListener::receiveSignal(
        cComponent *src, simsignal_t id, const SimTime& v)
{
    parentMod->emit(destSigId, v);
}
