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

package homatransport.dcntopo;

import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import ned.DatarateChannel;
import inet.networklayer.configurator.ipv4.IPv4NetworkConfigurator;
import homatransport.common.HostBase;
import homatransport.common.GlobalSignalListener;

network DcnTopo
{
    parameters:
        @display("bgb=319,325");
        xml topologyConfig = xmldoc("config.xml");
        int numServersPerTor;
        double edgeLinkDelay @unit(us) = default(0.0us); // This models NIC
                                                         // intrinsic delay
        double realLoadFactor;
        int nicLinkSpeed @unit(Gbps);

        int numTors;

        // models the fabrciLinkDealy plus the intrinsic switching delay
        double fabricLinkDelay @unit(us) = default(0.0us);

        // NOTE: always greater and equal to nicLinkSpeed
        int fabricLinkSpeed @unit(Gbps);

        // models the software trun around time, in the hosts, between the time
        // ethFrame is received until the notification is received in software
        // or when the pkt is send in software until it's received at the NIC.
        double hostSwTurnAroundTime @unit(us);

        // when non-zero, it models the intrinsic fixed switching delay
        // on the output ports of the routers
        double switchFixDelay @unit(us);

        // models NIC send think time in host nics.
        double hostNicSxThinkTime @unit(us);

        // must be greater than or equal to 1.0 (1.0 means full bisection bw)
        double overSubFactor = default(1.0);

        // Enables the cut-through switching wherever possible
        bool isFabricCutThrough = default(false);

        int numAggSwitches = (numServersPerTor * nicLinkSpeed) /
                (overSubFactor * fabricLinkSpeed);
        int numHosts = numServersPerTor * numTors;

        // Set parameters to enablle random spray multipath routing
        tor[*].networkLayer.ip.numUpLinks =  numAggSwitches;
        tor[*].networkLayer.ip.supportMultiPathRouting = true;
        aggRouter[*].networkLayer.ip.numUpLinks = 0;
        aggRouter[*].networkLayer.ip.supportMultiPathRouting = true;

        // set cutThrough for mac modules in the Switches
        tor[*].eth[*].mac.isReceiveOnStart = isFabricCutThrough;
        aggRouter[*].eth[*].mac.isReceiveOnStart = isFabricCutThrough;

        // Set input and output hooks in NICs to "Lagger" type and set the delayPar to
        // hostSwTurnAroundTime
        **.eth[*].numInputHooks = 1;
        **.eth[*].inputHook[*].typename = "Lagger";
        **.eth[*].inputHook[*].hookType = "InputHook";
        host[*].eth[*].inputHook[*].delayPar = convertUnit(hostSwTurnAroundTime, "s");
        **.eth[*].numOutputHooks = 1;
        **.eth[*].outputHook[*].typename = "Lagger";
        **.eth[*].outputHook[*].hookType = "OutputHook";
        host[*].eth[*].outputHook[*].delayPar = convertUnit(hostSwTurnAroundTime, "s");

    types:
        channel FabricChannel extends DatarateChannel
        {
            datarate = fabricLinkSpeed;
        }

        channel EdgeChannel extends DatarateChannel
        {
            datarate = nicLinkSpeed;
        }

    submodules:
        globalListener: GlobalSignalListener{}
        aggRouter[numAggSwitches]: Router {
            @display("i=abstract/router2");
            **.promiscuous = true;
        }

        tor[numTors]: Router {
            @display("i=abstract/router");
            **.promiscuous = true;
        }

        host[numHosts]: HostBase {
            @display("i=device/server2");
            hostConfig = xmldoc("config.xml", "/topologyConfig/hostConfig[@id=$MODULE_INDEX]");
            nicLinkSpeed = nicLinkSpeed;
            fabricLinkSpeed = fabricLinkSpeed;
            edgeLinkDelay = edgeLinkDelay;
            fabricLinkDelay = fabricLinkDelay;
            hostSwTurnAroundTime = hostSwTurnAroundTime;
            hostNicSxThinkTime = hostNicSxThinkTime;
            switchFixDelay = switchFixDelay;
            trafficGeneratorApp[*].isFabricCutThrough = isFabricCutThrough;
            trafficGeneratorApp[*].isSingleSpeedFabric = (nicLinkSpeed == fabricLinkSpeed);
        }

        configurator: IPv4NetworkConfigurator {
            parameters:
                config = xmldoc("config.xml", "/topologyConfig/IPv4Configurator/config");
                @display("p=26,25");
        }

    connections allowunconnected:
        for i=0..numTors-1, for j=0..numServersPerTor-1 {
            tor[i].ethg$o++ --> EdgeChannel{delay=(edgeLinkDelay+switchFixDelay);} --> host[i * numServersPerTor + j].ethg$i++;
            tor[i].ethg$i++ <-- EdgeChannel{delay=(edgeLinkDelay+hostNicSxThinkTime);} <-- host[i * numServersPerTor + j].ethg$o++;
        }

        for i=0..numAggSwitches-1, for j=0..numTors-1 {
            aggRouter[i].ethg++ <--> FabricChannel{delay=(fabricLinkDelay+switchFixDelay);} <--> tor[j].ethg++ ;
        }
}
