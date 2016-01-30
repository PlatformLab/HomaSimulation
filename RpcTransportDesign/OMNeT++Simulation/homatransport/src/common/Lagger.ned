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
import inet.common.IHook;

//
// This module is intended to be used as a hook at EthernetInterfaces.
// This module serves two purpose:
// 1. For every packet that arrives at the module, delays it for some fixed value.
// 2. When MAC module's recptionOnDeliveryStart flag is set to true in the
// EthernetInterface that this delayer belongs to, it means that the switch this
// module belongs to is cut-through. But the cut-through is not possible if the
// packet has come from a low speed link and is going to a higher speed link. In
// such case, we need to compensate and inject in a proper delay value. To
// achieve this, there are two types of input and output Delayer hook that both
// exist as a pair in upward and downward direction. the input hook will tag
// every received packet with the speed of the link the packet has arrived
// from. Then the output hook add the proper delay if the output link speed is
// different from the input link speed.
//

simple Lagger like IHook
{
    parameters:
        volatile double delayPar @unit(s) = default(0s);    // delay [seconds] e.g. uniform(20ms,40ms)
        string hookType; // should be "InputHook" or "OutputHook"
        @display("i=status/lightning");
    gates:
        input in;
        output out;
}
