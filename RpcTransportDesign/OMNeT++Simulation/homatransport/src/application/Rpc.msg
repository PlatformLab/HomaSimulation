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

cplusplus {{
#include "inet/networklayer/common/L3Address.h"
}}
class noncobject inet::L3Address;


//
// This object provides the medium for communicating between the RpcHandler and
// Transport layer. This object also keeps enough information for calculating
// and dumping stattistics like end to end delay and stretch of a message. 
//
packet Rpc {
     inet::L3Address destAddr;
     inet::L3Address srcAddr;
     simtime_t msgCreationTime;
     simtime_t transportSchedDelay;
     simtime_t transportSchedPreemptionLag;
     long msgBytesOnWire;
}
