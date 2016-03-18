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
// This module generates messages destined to a provided list of destinations.
// The message sizes are taken from distributions like DCTCP or Facebook
// key-value workload. This moduled then passes the message the rpc layer which
// properly handles the requests.
//
simple WorkloadSynthesizer like ITrafficGeneratorApp
{
    parameters:
        xml appConfig = default(xml("<isSender/>true<destIds/><destIds></destIds>"));

        // The path to the InterfaceTable module. Used to find the IpAddr of the
        // local host.
        string interfaceTableModule;
        int nicLinkSpeed @unit(Gbps);
        double loadFactor;

        // workload types can be from this set: {"DCTCP", "FACEBOOK_KEY_VALUE", "FACEBOOK_WEB_SERVER_INTRACLUSTER",
        // "FACEBOOK_CACHE_FOLLOWER_INTRACLUSTER", "FACEBOOK_HADOOP_ALL", "FABRICATED_HEAVY_MIDDLE",
        // "FABRICATED_HEAVY_HEAD"}
        string workloadType = default("DCTCP");
        string interArrivalDist = default("exponential");
        string destAddresses = default("");
        double startTime @unit(s) = default(0s);
        double stopTime @unit(s) = default(-1s);
        @display("i=block/app");

    gates:
        input rpcHandlerIn @label(AppMessage/up);
        output rpcHandlerOut @labels(AppMessage/down);
        output oracleSchedulerOut @loose @label(AppMessage/down);
}
