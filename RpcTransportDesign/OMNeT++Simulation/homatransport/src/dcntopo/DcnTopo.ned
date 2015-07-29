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

network DcnTopo
{
    parameters:
        xml topologyConfig = xmldoc("config.xml");
        int numServersPerTor;
        double edgeLinkDelay @unit(us) = default(0.0us);
        int nicLinkSpeed @unit(Gbps);

        int numTors;
        double fabricLinkDelay @unit(us) = default(0.0us);
        int fabricLinkSpeed @unit(Gbps); // greater and equal
                                         // to nicLinkSpeed

        double overSubFactor = default(1.0); // must be larger than 1.0
                                             // 1.0 means full bisection bw

        int numAggSwitches = (numServersPerTor * nicLinkSpeed) / 
                (overSubFactor * fabricLinkSpeed);


        tor[*].networkLayer.ip.numUpLinks =  numAggSwitches;
        tor[*].networkLayer.ip.supportMultiPathRouting = true;
        aggRouter[*].networkLayer.ip.supportMultiPathRouting = true;

        int numHosts = numServersPerTor * numTors;

        @display("bgb=319,325");

    types:
        channel FabricChannel extends DatarateChannel
        {
            datarate = fabricLinkSpeed;
            delay = fabricLinkDelay;
        }

        channel EdgeChannel extends DatarateChannel
        {
            datarate = nicLinkSpeed;
            delay = edgeLinkDelay;
        }

    submodules:
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
        }

        configurator: IPv4NetworkConfigurator {
            parameters:
                config = xmldoc("config.xml", "/topologyConfig/IPv4Configurator/config");
                @display("p=26,25");
        }

    connections allowunconnected:
        for i=0..numTors-1, for j=0..numServersPerTor-1 {
            tor[i].ethg++ <--> EdgeChannel <--> host[i * numServersPerTor + j].ethg++ ;
        }
        
        for i=0..numAggSwitches-1, for j=0..numTors-1 {
            aggRouter[i].ethg++ <--> FabricChannel <--> tor[j].ethg++ ;
        }

}
