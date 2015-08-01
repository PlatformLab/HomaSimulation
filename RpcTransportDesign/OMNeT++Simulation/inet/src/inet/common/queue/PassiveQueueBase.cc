//
// Copyright (C) 2005 Andras Varga
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include <algorithm>

#include "inet/common/queue/PassiveQueueBase.h"
#include "../../homatransport/src/transport/HomaPkt_m.h"

namespace inet {

simsignal_t PassiveQueueBase::rcvdPkSignal = registerSignal("rcvdPk");
simsignal_t PassiveQueueBase::enqueuePkSignal = registerSignal("enqueuePk");
simsignal_t PassiveQueueBase::dequeuePkSignal = registerSignal("dequeuePk");
simsignal_t PassiveQueueBase::dropPkByQueueSignal = registerSignal("dropPkByQueue");
simsignal_t PassiveQueueBase::queueingTimeSignal = registerSignal("queueingTime");


simsignal_t PassiveQueueBase::requestQueueingTimeSignal = registerSignal("requestQueueingTime");
simsignal_t PassiveQueueBase::grantQueueingTimeSignal = registerSignal("grantQueueingTime");
simsignal_t PassiveQueueBase::dataQueueingTimeSignal = registerSignal("dataQueueingTime");

void PassiveQueueBase::initialize()
{
    // state
    packetRequested = 0;
    WATCH(packetRequested);

    // statistics
    numQueueReceived = 0;
    numQueueDropped = 0;
    WATCH(numQueueReceived);
    WATCH(numQueueDropped);
}

void PassiveQueueBase::handleMessage(cMessage *msg)
{
    numQueueReceived++;

    emit(rcvdPkSignal, msg);

    if (packetRequested > 0) {
        packetRequested--;
        emit(enqueuePkSignal, msg);
        emit(dequeuePkSignal, msg);
        emit(queueingTimeSignal, SIMTIME_ZERO);
        cPacket* pkt = searchEncapHomaPkt(msg);
        if (pkt) {
            HomaPkt* homaPkt = check_and_cast<HomaPkt*>(pkt);
            switch (homaPkt->getPktType()) {
                case PktType::REQUEST:
                    emit(requestQueueingTimeSignal, SIMTIME_ZERO);
                    break;
                case PktType::GRANT:
                    emit(grantQueueingTimeSignal, SIMTIME_ZERO);
                    break;
                case PktType::DATA:
                    emit(grantQueueingTimeSignal, SIMTIME_ZERO);
                    break;
                default:
                    throw cRuntimeError("HomaPkt arrived at the queue has unknown type.");
            }
        }
        sendOut(msg);
    }
    else {
        msg->setArrivalTime(simTime());
        cMessage *droppedMsg = enqueue(msg);
        if (msg != droppedMsg)
            emit(enqueuePkSignal, msg);

        if (droppedMsg) {
            numQueueDropped++;
            emit(dropPkByQueueSignal, droppedMsg);
            delete droppedMsg;
        }
        else
            notifyListeners();
    }

    if (ev.isGUI()) {
        char buf[40];
        sprintf(buf, "q rcvd: %d\nq dropped: %d", numQueueReceived, numQueueDropped);
        getDisplayString().setTagArg("t", 0, buf);
    }
}

void PassiveQueueBase::requestPacket()
{
    Enter_Method("requestPacket()");

    cMessage *msg = dequeue();
    if (msg == NULL) {
        packetRequested++;
    }
    else {
        emit(dequeuePkSignal, msg);
        emit(queueingTimeSignal, simTime() - msg->getArrivalTime());
        cPacket* pkt = searchEncapHomaPkt(msg);
        if (pkt) {
            HomaPkt* homaPkt = check_and_cast<HomaPkt*>(pkt);
            switch (homaPkt->getPktType()) {
                case PktType::REQUEST:
                    emit(requestQueueingTimeSignal, simTime() - msg->getArrivalTime());
                    break;
                case PktType::GRANT:
                    emit(grantQueueingTimeSignal, simTime() - msg->getArrivalTime());
                    break;
                case PktType::DATA:
                    emit(grantQueueingTimeSignal, simTime() - msg->getArrivalTime());
                    break;
                default:
                    throw cRuntimeError("HomaPkt arrived at the queue has unknown type.");
            }
        }
        sendOut(msg);
    }
}

void PassiveQueueBase::clear()
{
    cMessage *msg;

    while (NULL != (msg = dequeue()))
        delete msg;

    packetRequested = 0;
}

cMessage *PassiveQueueBase::pop()
{
    return dequeue();
}

void PassiveQueueBase::finish()
{
}

void PassiveQueueBase::addListener(IPassiveQueueListener *listener)
{
    std::list<IPassiveQueueListener *>::iterator it = find(listeners.begin(), listeners.end(), listener);
    if (it == listeners.end())
        listeners.push_back(listener);
}

void PassiveQueueBase::removeListener(IPassiveQueueListener *listener)
{
    std::list<IPassiveQueueListener *>::iterator it = find(listeners.begin(), listeners.end(), listener);
    if (it != listeners.end())
        listeners.erase(it);
}

void PassiveQueueBase::notifyListeners()
{
    for (std::list<IPassiveQueueListener *>::iterator it = listeners.begin(); it != listeners.end(); ++it)
        (*it)->packetEnqueued(this);
}

cPacket*
PassiveQueueBase::searchEncapHomaPkt(cMessage* msg)
{
    cPacket* pkt = dynamic_cast<cPacket*>(msg);
    while (pkt) {
        if (dynamic_cast<HomaPkt*>(pkt)) {
            return pkt;
        }

        if (pkt->hasEncapsulatedPacket()) {
            pkt = dynamic_cast<cPacket*>(pkt->getEncapsulatedPacket());
        } else {
            break;
        }
    }
    return NULL;
}

} // namespace inet

