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
        bool isSender = default(true);
        string interfaceTableModule;   // The path to the InterfaceTable module
        int linkSpeed @unit(Gbps) = default(10Gbps);
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
        @signal[msgLifeTime](type=simtime_t);

        @statistic[sentMsg](title="messages sent"; source=sentMsg; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationMode=none);
        @statistic[rcvdMsg](title="received messages"; source=rcvdMsg; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationMode=none);
        @statistic[rcvdMsgLifeTime](title="received messages lifetime"; source="msgLifeTime"; unit=s; record=stats,histogram,vector; interpolationmode=none);
    gates:
        input transportIn @label(AppMessage/up);
        output transportOut @labels(AppMessage/down);
}
