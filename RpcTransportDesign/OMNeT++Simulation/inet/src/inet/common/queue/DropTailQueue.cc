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

#include "inet/common/INETDefs.h"

#include "inet/common/queue/DropTailQueue.h"

namespace inet {

Define_Module(DropTailQueue);

void DropTailQueue::initialize()
{
    PassiveQueueBase::initialize();

    queue.setName(par("queueName"));

    //statistics
    emit(queueLengthSignal, queue.length());
    emit(queueByteLengthSignal, queue.getByteLength());

    outGate = gate("out");

    // configuration
    frameCapacity = par("frameCapacity");
}

cMessage *DropTailQueue::enqueue(cMessage *msg)
{
    // update stats for queueLenOne if queue is empty. The fact that we are
    // queuing a packet, means a packet is already on the transmission so queue
    // len is in practice one in such cases even though queue.length() is zero.
    int pktOnWire = pktOnWireByteSize == 0 ? 0 : 1; 
    switch (queue.length()) {
        case 0:
            if (pktOnWire == 0) {
                queueEmpty++;
            } else if (pktOnWire == 1) {
                queueLenOne++;
            }
            break;
        case 1:
            if (pktOnWire == 0) {
                queueLenOne++;
            }
        default:
            break;
    }

    if (frameCapacity && queue.length() >= frameCapacity) {
        EV << "Queue full, dropping packet.\n";
        return msg;
    }

    else {
        // The timeavg of queueLength queueByteLength will not be accurate if
        // the queue(Byte)LengthSignal is emitted before a packet is stored at
        // the queue (similar to the following line) however the average of
        // queueLength at the packet arrival time at the queue will be accurate.
        // If you want the timeavg of queueLength instead, comment out the
        // following line and decomment the other emit invokations of
        // queueLengthSignal in this file.
        emit(queueLengthSignal, queue.length() + pktOnWire);
        emit(queueByteLengthSignal, queue.getByteLength() + pktOnWireByteSize);
        queue.insert(check_and_cast<cPacket*>(msg));
        //emit(queueLengthSignal, queue.length());
        //emit(queueByteLengthSignal, queue.getByteLength());
        return NULL;
    }
}

cMessage *DropTailQueue::dequeue()
{
    if (queue.empty())
        return NULL;

    cMessage *msg = (cMessage *)queue.pop();

    // statistics
    //emit(queueLengthSignal, queue.length());
    //emit(queueByteLengthSignal, queue.getByteLength());

    return msg;
}

void DropTailQueue::sendOut(cMessage *msg)
{
    send(msg, outGate);
}

bool DropTailQueue::isEmpty()
{
    return queue.empty();
}

} // namespace inet

