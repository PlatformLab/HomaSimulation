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

package homatransport.transport;
import homatransport.transport.ITransportScheme;

//
// Defines the skeleton of a minimal near optimal priority based transport
// scheme. The metrics of optimality is minimizing the average flow completion
// time for the message in the datacenter.
//
simple PseudoIdealPriorityTransport like ITransportScheme
{
    parameters:
        xml transportConfig = default(xml("<param/>"));
        int localPort = default(-1);
        int destPort;
        int nicLinkSpeed @unit(Gbps) = default(10Gbps);

        @signal[msgsLeftToSend](type=long);
        @signal[bytesLeftToSend](type=long);
        @statistic[msgsLeftToSend](title="num msgs left to send"; source=msgsLeftToSend; record=stats, histogram; interpolationmode=none);
        @statistic[bytesLeftToSend](title="num total bytes left to send"; source=bytesLeftToSend; record=stats, histogram; interpolationmode=none);

    gates:
        input appIn[] @labels(AppMessage/down);
        input udpIn @labels(UDPControlInfo/up);
        output appOut[] @labels(AppMessage/up);
        output udpOut @labels(UDPControlInfo/down);
}
