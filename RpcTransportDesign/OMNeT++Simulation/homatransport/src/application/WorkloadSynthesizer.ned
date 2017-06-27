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
package homatransport.application;
import homatransport.application.ITrafficGeneratorApp;

//
// This module generates messages with sizes taken from distributions like DCTCP
// or Facebook key-value workload and passes the generated messages to the
// transport layer to the transport protocol.
//
simple WorkloadSynthesizer like ITrafficGeneratorApp
{
    parameters:
        xml appConfig = default(xml("<isSender/>true<destIds/><destIds></destIds>"));
        string interfaceTableModule;   // The path to the InterfaceTable module

        // a space sperated list of msg sizes in bytes in a string like "100 1000 10000 100000"
        // that creates bins of msg size renges. each number in this string is an upper bound in the range;
        // Any msgeSize that doesn't fit in any bins sepcified in the string, will be placed in an additional distinct bin.
        string msgSizeRanges;

        int nicLinkSpeed @unit(Gbps);
        int fabricLinkSpeed @unit(Gbps);
        double fabricLinkDelay @unit(us);
        double edgeLinkDelay @unit(us);
        double hostSwTurnAroundTime @unit(us);
        double hostNicSxThinkTime @unit(us);
        double switchFixDelay @unit(us);
        bool isFabricCutThrough;
        bool isSingleSpeedFabric;
        double loadFactor;

        // workload types from this set: {"DCTCP", "FACEBOOK_KEY_VALUE"}
        string workloadType = default("DCTCP");
        string interArrivalDist = default("exponential");
        string destAddresses = default("");
        double startTime @unit(s) = default(0s);
        double stopTime @unit(s) = default(-1s);
        @display("i=block/app");

        @signal[sentMsg](type=cPacket);
        @signal[rcvdMsg](type=cPacket);
        @signal[msgE2EDelay](type=simtime_t);

        @statistic[sentMsg](title="messages sent"; source=sentMsg; record=count,"last(sumPerDuration(packetBytes))","sum(packetBytes)","vector(packetBytes)"; interpolationMode=none);
        @statistic[rcvdMsg](title="received messages"; source=rcvdMsg; record=count,"last(sumPerDuration(packetBytes))","sum(packetBytes)","vector(packetBytes)"; interpolationMode=none);
        @statistic[msgE2EDelay](title="Message lifetime (end to end latency)"; source="msgE2EDelay"; unit=s; record=count,stats,histogram,vector; interpolationmode=none);

        @signal[msg*E2EDelay](type=simtime_t);
        @signal[msg*E2EStretch](type=double);
        @signal[msg*QueuingDelay](type=simtime_t);
        @signal[msg*TransportSchedDelay](type=simtime_t);
        @signal[msg*BytesOnWire](type=AppMessage);
        // This singal is emitted with a pointer to an object that contains
        // values of all other msg** signals above and the message size those
        // values correspoond to.
        @signal[mesgStats];
        // This signal is meant to track the changes in number of senders
        // actively receive  grant from the receiver and time duration during
        // which that number was maintained
        @signal[activeScheds];

        @statisticTemplate[msgRangesE2EDelay](title="msg lifetime"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statisticTemplate[msgRangesE2EStretch](title="msg stretch"; record=count,stats,histogram; interpolationmode=none);
        @statisticTemplate[msgRangesQueuingDelay](title="msg queuing delay";unit=s; record=count,stats,histogram; interpolationmode=none);
        @statisticTemplate[msgRangesTransportSchedDelay](title="transport scheduling delay";unit=s; record=count,stats,histogram; interpolationmode=none);
        @statisticTemplate[msgRangesBytesOnWire](title="total bytes on wire";record=stats; interpolationmode=none);

    gates:
        input transportIn @label(AppMessage/up);
        output transportOut @labels(AppMessage/down);
}
