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
#include "inet/linklayer/ethernet/EtherMACBase.h"
#include "inet/linklayer/ethernet/Ethernet.h"

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
    if (frameCapacity && queue.length() >= frameCapacity) {
        EV << "Queue full, dropping packet.\n";
        return msg;
    }

    // update stats for queueLenOne if queue is empty. The fact that we are
    // queuing a packet, means a packet is already on the transmission so in
    // practice  queue len is one in such cases even though queue.length() is
    // zero.
    if (queue.length() == 0) {
        queueLenOne++;
    }

    // The timeavg of queueLength queueByteLength will not be accurate if
    // the queue(Byte)LengthSignal is emitted before a packet is stored at
    // the queue (similar to the following line) however the average of
    // queueLength at the packet arrival time at the queue will be accurate.
    // If you want the timeavg of queueLength instead, comment out the
    // following line and decomment the other emit invokations of
    // queueLengthSignal in this file.
    emit(queueLengthSignal, queue.length() + 1);
    emit(queueByteLengthSignal, queue.getByteLength() + lastTxPktBytes);
    cPacket* pkt = check_and_cast<cPacket*>(msg);
    queue.insert(pkt);
    txPktSizeAtArrival[pkt] = lastTxPktBytes;

    //emit(queueLengthSignal, queue.length());
    //emit(queueByteLengthSignal, queue.getByteLength());
    return NULL;
}

cMessage *DropTailQueue::dequeue()
{
    if (queue.empty())
        return NULL;

    cPacket* pkt = queue.pop();

    // statistics
    //emit(queueLengthSignal, queue.length());
    //emit(queueByteLengthSignal, queue.getByteLength());

    return (cMessage *)pkt;
}

void DropTailQueue::sendOut(cMessage *msg)
{
    send(msg, outGate);
}

bool DropTailQueue::isEmpty()
{
    return queue.empty();
}

simtime_t
DropTailQueue::txPktDurationAtArrival(cPacket* pktToSend)
{

    double lastTxBits = 0.0;
    lastTxBits = txPktSizeAtArrival.at(pktToSend) * 8.0 + INTERFRAME_GAP_BITS;
    txPktSizeAtArrival.erase(pktToSend);

    cModule* parentModule = getParentModule(); 
    if (!parentModule->hasGate("out")) {
        return SIMTIME_ZERO;
    }

    cGate* parentOutGate = parentModule->gate("out");
    cGate* destGate = parentOutGate->getNextGate();
    cModule* nextModule = destGate->getOwnerModule();

    if (strcmp(nextModule->getName(), "mac") != 0) {
        return SIMTIME_ZERO;
    }

    return lastTxBits / dynamic_cast<EtherMACBase*>(nextModule)->getTxRate();
}

} // namespace inet

