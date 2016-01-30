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

//
// Interface for applications
//
moduleinterface ITrafficGeneratorApp
{
    parameters:
        @display("i=block/app");
        xml appConfig;
        int nicLinkSpeed @unit(Gbps);
        int fabricLinkSpeed @unit(Gbps);
        double fabricLinkDelay @unit(us);
        double edgeLinkDelay @unit(us);
        double hostSwTurnAroundTime @unit(us);
        double hostNicSxThinkTime @unit(us);
        double switchFixDelay @unit(us);

    gates:
        input transportIn @label(AppMessage/up);
        output transportOut @labels(AppMessage/down);
}
