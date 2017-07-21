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
#include <vector>
#include "inet/networklayer/common/L3Address.h"
typedef std::vector<uint32_t> UnsignedVec;
}}
class noncobject inet::L3Address;
class noncobject UnsignedVec;

enum PktType
{
    REQUEST = 0;
    GRANT = 1;
    SCHED_DATA = 2;
    UNSCHED_DATA = 3;
};

struct UnschedFields
{
    // Unsced pkts contain duplicate information of request pkt plus the range
    // of bytes they carry.
    uint32_t msgByteLen;
    simtime_t msgCreationTime;

    // Total number of unscheduled bytes sent for this message. ie. the bytes
    // sent in this unsched plus the bytes sent in every other unsched packets.
    uint16_t totalUnschedBytes;

    // the index of first and last byte data bytes that trailing this unsched.
    // packet.
    uint32_t firstByte;
    uint32_t lastByte;

    // the vector of [prio, unsched, prio, unsched, ...] pakcets pairs. Each two
    // respective elements of the vector specifies a priority and unsched bytes
    // on that priority except the unsched bytes this packet.
    UnsignedVec prioUnschedBytes;
};

struct GrantFields
{
    uint16_t grantBytes;
    uint32_t offset;
    uint16_t schedPrio;

    // [(uint32_t size_upper_bound, uint16_t req_byte, uint16_t prio)]
    UnsignedVec sizeReqBytesPrio;

    // [(uint32_t size_upper_bound, uint16_t unsched_byte, uint16_t prio)]
    UnsignedVec sizeUnschedBytesPrio;
};

struct SchedDataFields
{
    // the index of first and last byte data bytes that trailing this sched.
    // data packet.
    uint32_t firstByte;
    uint32_t lastByte;
};

//
// generated message class
//
packet HomaPkt {
    @customize(true);

    inet::L3Address srcAddr;
    inet::L3Address destAddr;
    uint64_t msgId;
    uint32_t priority = 1;
    int pktType @enum(PktType);

    // depending on the pktType, only one of the three following struct are
    // relevant in the context of a HomaPkt.
    UnschedFields unschedFields;    // Used for both request and unsched
    GrantFields grantFields;            // Used only for grants
    SchedDataFields schedDataFields;    // Used only for scheduled data
}
