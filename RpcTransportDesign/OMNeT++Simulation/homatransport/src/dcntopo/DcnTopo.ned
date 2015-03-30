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

module Rack
{
    parameters:
        double linkDelay @unit(us);
        int nicLinkSpeed @unit(Gbps);
        int numServersPerTor;
        @display("bgb=394,286");
    gates:
        inout upLink[];

    types:
        channel EdgeChannel extends DatarateChannel
        {
            datarate = nicLinkSpeed;
            delay = linkDelay;
        }

    submodules:
        tor: Router {
            parameters:
                @display("p=177,170");
        }

        host[numServersPerTor]: StandardHost {
            parameters:
                @display("i=device/server2");
        }

    connections:
        for i=0..numServersPerTor-1 {
            tor.ethg++ <--> EdgeChannel <--> host[i].ethg++;
        }

        for i=0..sizeof(upLink)-1 {
            tor.ethg++ <--> upLink[i];
        }
}

network DcnTopo
{
    parameters:
        int numServersPerTor = default(16);
        int nicLinkSpeed @unit(Gbps) = default(10Gbps);

        int numTors = default(2);
        int fabricLinkSpeed @unit(Gbps) = default(40Gbps); // greater and equal
                                                           // to nicLinkSpeed
        double fabricLinkDelay @unit(us) = default(0.2us);
        int numAggSwitches = default(2);
        double overSubFactor = default(1.0); // must be larger than 1.0
                                             // 1.0 means full bisection bw

        rack[*].numServersPerTor = numServersPerTor;
        rack[*].nicLinkSpeed = nicLinkSpeed;
        rack[*].linkDelay = default(0.05us);

        int numAggLinksPerTor = numServersPerTor * nicLinkSpeed / 
                (overSubFactor* numAggSwitches * fabricLinkSpeed);


        @display("bgb=319,325");
    types:
        channel FabricChannel extends DatarateChannel
        {
            datarate = fabricLinkSpeed;
            delay = fabricLinkDelay;
        }

    submodules:
        aggRouter[numAggSwitches]: Router {
            @display("i=abstract/router2");
        }
        rack[numTors]: Rack {
            @display("i=misc/city");
        }
        configurator: IPv4NetworkConfigurator {
            parameters:
                @display("p=26,25");
        }

    connections allowunconnected:
        for i=0..numTors-1, for j=0..numAggSwitches-1,
                    for k=0..numAggLinksPerTor-1 {
            rack[i].upLink++ <--> FabricChannel <--> aggRouter[j].ethg++;
        }
}
