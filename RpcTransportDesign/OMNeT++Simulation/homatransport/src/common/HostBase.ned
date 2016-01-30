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

package homatransport.common;

import inet.node.inet.NodeBase;
import inet.applications.ITCPApp;
import inet.applications.IUDPApp;
import inet.transportlayer.ITCP;
import inet.transportlayer.IUDP;
import homatransport.application.ITrafficGeneratorApp;
import homatransport.transport.ITransportScheme;

//
// A simplified version of StandardHost module from inet package. Many
// unnecessary parts are trimmed off and TransportScheme has been added.
//

module HostBase extends NodeBase
{
    parameters:
        @display("i=device/pc2");
        xml hostConfig;
        int nicLinkSpeed @unit(Gbps);
        int fabricLinkSpeed @unit(Gbps);

        int numTrafficGeneratorApp = default(0);
        // no of TCP apps. Specify the app types in INI file with
        // tcpApp[0..1].typename="TCPEchoApp" syntax
        int numTcpApps = default(0);

        // no of UDP apps. Specify the app types in INI file with
        // udpApp[0..1].typename="UDPVideoStreamCli" syntax
        int numUdpApps = default(0);
        bool hasTransportScheme = default(numTrafficGeneratorApp>0);
        bool hasTcp = default(numTcpApps>0);
        bool hasUdp = default((numUdpApps>0) || (numTrafficGeneratorApp>0));

        // tcp implementation (e.g. ~TCP, ~TCP_lwIP, ~TCP_NSC) or ~TCPSpoof
        string tcpType =
            default(firstAvailable("TCP", "TCP_lwIP", "TCP_NSC", "TCP_None"));
        string udpType = default(firstAvailable("UDP","UDP_None"));
        string transportSchemeType =
                default(firstAvailable("HomaTransport", "TransportSchemeNone"));
        forwarding = default(false);  // disable routing by default
        networkLayer.proxyARP = default(false);

        double edgeLinkDelay @unit(us);
        double fabricLinkDelay @unit(us);
        double hostSwTurnAroundTime @unit(us);
        double hostNicSxThinkTime @unit(us);
        double switchFixDelay @unit(us);

    submodules:
        trafficGeneratorApp[numTrafficGeneratorApp]: <default("WorkloadSynthesizer")> like ITrafficGeneratorApp {
            parameters:
                @display("p=490,38");
                appConfig = xmldoc("config.xml", "/topologyConfig/hostConfig[@id=$PARENTMODULE_INDEX]/appConfig");
                nicLinkSpeed = nicLinkSpeed;
                fabricLinkSpeed = fabricLinkSpeed;
                edgeLinkDelay = edgeLinkDelay;
                fabricLinkDelay = fabricLinkDelay;

                hostSwTurnAroundTime = hostSwTurnAroundTime;
                hostNicSxThinkTime = hostNicSxThinkTime;
                switchFixDelay = switchFixDelay;
        }

        transportScheme: <transportSchemeType> like ITransportScheme if hasTransportScheme {
            parameters:
                @display("p=445,112");
                transportConfig = xmldoc("config.xml", "/topologyConfig/hostConfig[@id=$PARENTMODULE_INDEX]/transportConfig");
                nicLinkSpeed = nicLinkSpeed;
        }

        udpApp[numUdpApps]: <> like IUDPApp {
            parameters:
                @display("p=308,110,row,60");
        }

        udp: <udpType> like IUDP if hasUdp {
            parameters:
                @display("p=394,176");
        }

        tcpApp[numTcpApps]: <> like ITCPApp {
            parameters:
                @display("p=147,54,row,60");
        }

        tcp: <tcpType> like ITCP if hasTcp {
            parameters:
                @display("p=147,141");
        }

    connections allowunconnected:
        for i=0..numTrafficGeneratorApp-1 {
            trafficGeneratorApp[i].transportOut --> transportScheme.appIn++;
            trafficGeneratorApp[i].transportIn <-- transportScheme.appOut++;
        }

        transportScheme.udpOut --> udp.appIn++ if  hasTransportScheme;
        transportScheme.udpIn <-- udp.appOut++ if hasTransportScheme;

        for i=0..numUdpApps-1 {
            udpApp[i].udpOut --> udp.appIn++;
            udpApp[i].udpIn <-- udp.appOut++;
        }

        udp.ipOut --> networkLayer.transportIn++ if hasUdp;
        udp.ipIn <-- networkLayer.transportOut++ if hasUdp;

        for i=0..numTcpApps-1 {
            tcpApp[i].tcpOut --> tcp.appIn++;
            tcpApp[i].tcpIn <-- tcp.appOut++;
        }

        tcp.ipOut --> networkLayer.transportIn++ if hasTcp;
        tcp.ipIn <-- networkLayer.transportOut++ if hasTcp;
}

