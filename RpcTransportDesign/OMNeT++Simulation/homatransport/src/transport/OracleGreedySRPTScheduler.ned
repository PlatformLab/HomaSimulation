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
// Defines the skeleton of a centralized scheduler that implements SRPT
// policy for messages that are to be transmitted on the network using
// a greedy approach. Every time a new message is arrived at one node in
// the network or a meesage depart the network, this centralized scheduler
// recalculates the decision and enforces new the transmissions in the sender
// nodes in the network.
//
simple OracleGreedySRPTScheduler {
    parameters:
        xml transportConfig = default(xml("<param/>"));
        int nicLinkSpeed @unit(Gbps) = default(10Gbps);
    gates:
        input appIn[] @loose @label(AppMessage/down);
}
