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
// defines a credit based receiver side driven transport protocol 
//
simple HomaTransport like ITransportScheme
{
    parameters:
        int linkSpeed @unit(Gbps) = default(10Gbps);
        int grantMinBytes @unit(byte) = default(500B);
        int localPort = default(-1);
        int destPort;

        @signal[sentPkt](type=cPacket);
        @signal[rcvdPkt](type=cPacket);
        @statistic[rcvdPkt](title="packets received"; source=rcvdPkt; record=count, "sum(packetBytes)", "vector(packetBytes)"; interpolationmode=none);
        @statistic[sentPkt](title="packets sent"; source=sentPkt; record=count, "sum(packetBytes)", "vector(packetBytes)"; interpolationmode=none);

    gates:
        input appIn[] @labels(AppMessage/down);
        input udpIn @labels(UDPControlInfo/up);
        output appOut[] @labels(AppMessage/up);
        output udpOut @labels(UDPControlInfo/down);
}
