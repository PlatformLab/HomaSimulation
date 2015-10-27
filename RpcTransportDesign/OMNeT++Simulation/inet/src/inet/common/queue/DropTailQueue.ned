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


package inet.common.queue;

//
// Drop-tail queue, to be used in network interfaces.
// Conforms to the ~IOutputQueue interface.
//
simple DropTailQueue like IOutputQueue
{
    parameters:
        int frameCapacity = default(0);
        string queueName = default("l2queue"); // name of the inner cQueue object, used in the 'q' tag of the display string
        string transportType = default(""); // <Behnam> can be "HomaTransport" or "PseudoIdealTransport".
                                            // Determines which priority scheduling function should be used.
        @display("i=block/queue");
        @signal[rcvdPk](type=cPacket);
        @signal[enqueuePk](type=cPacket);
        @signal[dequeuePk](type=cPacket);
        @signal[dropPkByQueue](type=cPacket);
        @signal[queueingTime](type=simtime_t; unit=s);
        @signal[queueLength](type=long);
        @signal[queueByteLength](type=long);

        // Queuing time for different pkt types
        @signal[requestQueueingTime](type=simtime_t; unit=s);
        @signal[grantQueueingTime](type=simtime_t; unit=s);
        @signal[schedDataQueueingTime](type=simtime_t; unit=s);
        @signal[unschedDataQueueingTime](type=simtime_t; unit=s);


        @statistic[rcvdPk](title="received packets"; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[dropPk](title="dropped packets"; source=dropPkByQueue; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[queueLength](title="queue length"; record=stats,timeavg,vector; interpolationmode=sample-hold);
        @statistic[queueByteLength](title="queue byte length"; record=stats,timeavg,histogram; interpolationmode=sample-hold);
        @statistic[queueingTime](title="queueing time"; record=stats,histogram,vector; interpolationmode=none);

        // Queuing time for different pkt types
        @statistic[requestQueueingTime](title="request pkts queueing time";source=requestQueueingTime; record=stats,histogram,vector; interpolationmode=none);
        @statistic[grantQueueingTime](title="grant pkts queueing time";source=grantQueueingTime; record=stats,histogram,vector; interpolationmode=none);
        @statistic[schedDataQueueingTime](title="sched. data pkts queueing time";source=schedDataQueueingTime; record=stats,histogram,vector; interpolationmode=none);
        @statistic[unschedDataQueueingTime](title="unsched. data pkts queueing time";source=unschedDataQueueingTime; record=stats,histogram,vector; interpolationmode=none);

    gates:
        input in;
        output out;
}

