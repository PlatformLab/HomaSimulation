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
        int nicLinkSpeed @unit(Gbps);
        int fabricLinkSpeed @unit(Gbps);
        double loadFactor = default(0.5);

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

        @signal[msg1PktE2EDelay](type=simtime_t);
        @signal[msg3PktsE2EDelay](type=simtime_t);
        @signal[msg6PktsE2EDelay](type=simtime_t);
        @signal[msg13PktsE2EDelay](type=simtime_t);
        @signal[msg33PktsE2EDelay](type=simtime_t);
        @signal[msg133PktsE2EDelay](type=simtime_t);
        @signal[msg1333PktsE2EDelay](type=simtime_t);
        @signal[msgHugeE2EDelay](type=simtime_t);

        @signal[msg1PktE2EStretch](type=double);
        @signal[msg3PktsE2EStretch](type=double);
        @signal[msg6PktsE2EStretch](type=double);
        @signal[msg13PktsE2EStretch](type=double);
        @signal[msg33PktsE2EStretch](type=double);
        @signal[msg133PktsE2EStretch](type=double);
        @signal[msg1333PktsE2EStretch](type=double);
        @signal[msgHugeE2EStretch](type=double);

        @signal[msg1PktQueuingDelay](type=simtime_t);
        @signal[msg3PktsQueuingDelay](type=simtime_t);
        @signal[msg6PktsQueuingDelay](type=simtime_t);
        @signal[msg13PktsQueuingDelay](type=simtime_t);
        @signal[msg33PktsQueuingDelay](type=simtime_t);
        @signal[msg133PktsQueuingDelay](type=simtime_t);
        @signal[msg1333PktsQueuingDelay](type=simtime_t);
        @signal[msgHugeQueuingDelay](type=simtime_t);

        @statistic[sentMsg](title="messages sent"; source=sentMsg; record=count,"last(sumPerDuration(packetBytes))","sum(packetBytes)","vector(packetBytes)"; interpolationMode=none);
        @statistic[rcvdMsg](title="received messages"; source=rcvdMsg; record=count,"last(sumPerDuration(packetBytes))","sum(packetBytes)","vector(packetBytes)"; interpolationMode=none);
        @statistic[msgE2EDelay](title="Message lifetime (end to end latency)"; source="msgE2EDelay"; unit=s; record=count,stats,histogram,vector; interpolationmode=none);

        @statistic[msg1PktE2EDelay](title="1 pkt size msg lifetime"; source="msg1PktE2EDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg3PktsE2EDelay](title="2 to 3 pkts msg lifetime"; source="msg3PktsE2EDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg6PktsE2EDelay](title="4 to 6 pkts msg lifetime"; source="msg6PktsE2EDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg13PktsE2EDelay](title="7 to 13 pkts msg lifetime"; source="msg13PktsE2EDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg33PktsE2EDelay](title="14 to 33 pkts msg lifetime"; source="msg33PktsE2EDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg133PktsE2EDelay](title="34 to 133 pkts msg lifetime"; source="msg133PktsE2EDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg1333PktsE2EDelay](title="134 to 1333 pkts msg lifetime"; source="msg1333PktsE2EDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msgHugeE2EDelay](title="more than 1334 pkts msg lifetime"; source="msgHugeE2EDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);

        @statistic[msg1PktE2EStretch](title="1 pkt size msg stretch"; source="msg1PktE2EStretch"; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg3PktsE2EStretch](title="2 to 3 pkts msg stretch"; source="msg3PktsE2EStretch"; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg6PktsE2EStretch](title="4 to 6 pkts msg stretch"; source="msg6PktsE2EStretch"; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg13PktsE2EStretch](title="7 to 13 pkts msg stretch"; source="msg13PktsE2EStretch"; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg33PktsE2EStretch](title="14 to 33 pkts msg stretch"; source="msg33PktsE2EStretch"; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg133PktsE2EStretch](title="34 to 133 pkts msg stretch"; source="msg133PktsE2EStretch"; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg1333PktsE2EStretch](title="134 to 1333 pkts msg stretch"; source="msg1333PktsE2EStretch"; record=count,stats,histogram; interpolationmode=none);
        @statistic[msgHugeE2EStretch](title="more than 1334 pkts msg stretch"; source="msgHugeE2EStretch"; record=count,stats,histogram; interpolationmode=none);

        @statistic[msg1PktQueuingDelay](title="1 pkt size msg queuing delay"; source="msg1PktQueuingDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg3PktsQueuingDelay](title="2 to 3 pkts msg queuing delay"; source="msg3PktsQueuingDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg6PktsQueuingDelay](title="4 to 6 pkts msg queuing delay"; source="msg6PktsQueuingDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg13PktsQueuingDelay](title="7 to 13 pkts msg queuing delay"; source="msg13PktsQueuingDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg33PktsQueuingDelay](title="14 to 33 pkts msg queuing delay"; source="msg33PktsQueuingDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg133PktsQueuingDelay](title="34 to 133 pkts msg queuing delay"; source="msg133PktsQueuingDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msg1333PktsQueuingDelay](title="134 to 1333 pkts msg queuing delay"; source="msg1333PktsQueuingDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);
        @statistic[msgHugeQueuingDelay](title="more than 1334 pkts msg queuing delay"; source="msgHugeQueuingDelay"; unit=s; record=count,stats,histogram; interpolationmode=none);


    gates:
        input transportIn @label(AppMessage/up);
        output transportOut @labels(AppMessage/down);
}
