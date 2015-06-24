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

enum PktType
{
    REQUEST = 0;
    GRANT = 1;
    DATA = 3;
};


struct RequestFields 
{
    uint32_t msgByteLen;
    simtime_t msgCreationTime;
};

struct GrantFields
{
    uint32_t grantBytes;
};

struct DataFields
{
    uint32_t firstByte;
    uint32_t lastByte;
};

//
// generated message class
//
packet HomaPkt {
    inet::L3Address srcAddr; 
    inet::L3Address destAddr;
    uint64_t msgId;
    int pktType @enum(PktType);   
    
    // depending on the pktType, only one of the three following struct are
    // relevant in the context of a HomaPkt.
    RequestFields reqFields;
    GrantFields grantFields;
    DataFields dataFields;
}
