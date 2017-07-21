/*
 * HomaPkt.cc
 *
 *  Created on: Sep 16, 2015
 *      Author: behnamm
 */


#include "HomaPkt.h"
#include "transport/HomaTransport.h"

Register_Class(HomaPkt);



HomaPkt::HomaPkt(HomaTransport* ownerTransport, const char *name, int kind)
    : HomaPkt_Base(name,kind)
{
    this->ownerTransport = ownerTransport;
}

HomaPkt::HomaPkt(const HomaPkt& other)
    : HomaPkt_Base(other)
{
    copy(other);
}

HomaPkt&
HomaPkt::operator=(const HomaPkt& other)
{
    if (this==&other) return *this;
    HomaPkt_Base::operator=(other);
    copy(other);
    return *this;
}

void
HomaPkt::copy(const HomaPkt& other)
{
    this->ownerTransport = other.ownerTransport;
}

HomaPkt*
HomaPkt::dup() const
{
    return new HomaPkt(*this);
}

uint32_t
HomaPkt::getFirstByte() const
{
    switch (getPktType()) {
        case PktType::SCHED_DATA:
            return getSchedDataFields().firstByte;
        case PktType::UNSCHED_DATA:
        case PktType::REQUEST:
            return getUnschedFields().firstByte;
        default:
            return 0;
    }
}

uint32_t
HomaPkt::headerSize()
{
    uint32_t size = sizeof(msgId_var);
    switch(getPktType()) {
        case PktType::REQUEST:
        case PktType::UNSCHED_DATA:
            size += (sizeof(getUnschedFields().msgByteLen) +
                sizeof(getUnschedFields().totalUnschedBytes) +
                sizeof(getUnschedFields().firstByte) +
                sizeof(getUnschedFields().lastByte)) +
                sizeof(decltype(
                    getUnschedFields().prioUnschedBytes)::value_type)*2;
            break;
        case PktType::GRANT:
            size += (sizeof(getGrantFields().grantBytes) +
                sizeof(getGrantFields().offset) +
                sizeof(getGrantFields().schedPrio) +
                getGrantFields().sizeReqBytesPrio.size() *
                sizeof(decltype(getGrantFields().sizeReqBytesPrio)::value_type)+
                getGrantFields().sizeUnschedBytesPrio.size() *
                sizeof(decltype(
                getGrantFields().sizeUnschedBytesPrio)::value_type));
            break;

        case PktType::SCHED_DATA:
            size += (sizeof(getSchedDataFields().firstByte) +
                    sizeof(getSchedDataFields().lastByte));
            break;
        default:
            break;
    }
    return size;
}

uint32_t
HomaPkt::getDataBytes()
{
    switch (this->getPktType()) {
        case PktType::REQUEST:
        case PktType::UNSCHED_DATA:
            return this->getUnschedFields().lastByte -
                this->getUnschedFields().firstByte + 1;
        case PktType::SCHED_DATA:
            return this->getSchedDataFields().lastByte -
                this->getSchedDataFields().firstByte + 1;
        case PktType::GRANT:
            return 0;
        default:
            throw cRuntimeError("PktType %d not defined", this->getPktType());
    }
    return 0;
}

cPacket*
HomaPkt::searchEncapHomaPkt(cPacket* pkt)
{
    while (pkt) {
        if (dynamic_cast<HomaPkt*>(pkt)) {
            return pkt;
        }

        if (pkt->hasEncapsulatedPacket()) {
            pkt = dynamic_cast<cPacket*>(pkt->getEncapsulatedPacket());
        } else {
            break;
        }
    }
    return NULL;
}

int
HomaPkt::comparePrios(cObject* obj1, cObject* obj2)
{
    cPacket* pkt1 = searchEncapHomaPkt(dynamic_cast<cPacket*>(obj1));
    cPacket* pkt2 = searchEncapHomaPkt(dynamic_cast<cPacket*>(obj2));
    if (!pkt1) {
        if (!pkt2) {
            return 0;
        } else {
            return -1;
        }
    } else if (!pkt2) {
        return 1;
    }

    HomaPkt* homaPkt1 = check_and_cast<HomaPkt*>(pkt1);
    HomaPkt* homaPkt2 = check_and_cast<HomaPkt*>(pkt2);
    if (homaPkt1->getPriority() < homaPkt2->getPriority()) {
        return -1;
    } else if (homaPkt1->getPriority() > homaPkt2->getPriority()) {
        return 1;
    } else {
        return 0;
    }
}

int
HomaPkt::compareSizeAndPrios(cObject* obj1, cObject* obj2)
{
    cPacket* pkt1 = searchEncapHomaPkt(dynamic_cast<cPacket*>(obj1));
    cPacket* pkt2 = searchEncapHomaPkt(dynamic_cast<cPacket*>(obj2));
    if (!pkt1) {
        if (!pkt2) {
            return 0;
        } else {
            return -1;
        }
    } else if (!pkt2) {
        return 1;
    }

    HomaPkt* homaPkt1 = check_and_cast<HomaPkt*>(pkt1);
    HomaPkt* homaPkt2 = check_and_cast<HomaPkt*>(pkt2);

    if ((homaPkt1->getPktType() != PktType::UNSCHED_DATA) ||
            (homaPkt2->getPktType() != PktType::UNSCHED_DATA))  {
        return -1;
    }

    uint32_t prio1 = homaPkt1->getPriority();
    uint32_t prio2 = homaPkt2->getPriority();
    uint32_t msgByteLen1 = homaPkt1->getUnschedFields().msgByteLen;
    uint32_t msgByteLen2 = homaPkt2->getUnschedFields().msgByteLen;
    if (msgByteLen1 < msgByteLen2) {
        return -1;
    }
    if ((msgByteLen1 == msgByteLen2) && (prio1 > prio2)) {
        return -1;
    }
    if ((msgByteLen1 == msgByteLen2) && (prio1 == prio2)) {
        return 0;
    }
    return 1;
}

/**
 *
 * For a message of size numDataBytes comprised of packets of type homaPktType,
 * this function returns the actual bytes transmitted on the wire. Both data
 * bytes packed in the packet and the header size of the packet are cosidered in
 * the calculation.
 *
 */
uint32_t
HomaPkt::getBytesOnWire(uint32_t numDataBytes, PktType homaPktType)
{
    HomaPkt homaPkt = HomaPkt();
    homaPkt.setPktType(homaPktType);

    // FIXME: Using this function can cause bugs in general since HomaPkt
    // header size can be different for two pkts of same type (eg.
    // grant pkts). Care must be taken when using this function. Solution:
    // Better to have a version of this function that takes in a poiter to the
    // pkt and uses the real header size of the packet to do calcualtion when
    // possible. This requires to find all invokations of this function and
    // replace them with the new function when possible.  
    uint32_t homaPktHdrSize = homaPkt.headerSize();

    uint32_t bytesOnWire = 0;

    uint32_t maxDataInHomaPkt = MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE
            - UDP_HEADER_SIZE - homaPktHdrSize;

    uint32_t numFullPkts = numDataBytes / maxDataInHomaPkt;
    bytesOnWire += numFullPkts * (MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_HDR_SIZE
            + ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP);

    uint32_t numPartialBytes = numDataBytes - numFullPkts * maxDataInHomaPkt;

    // if all numDataBytes fit in full pkts, we should return at this point
    if (numFullPkts > 0 && numPartialBytes == 0) {
        return bytesOnWire;
    }

    numPartialBytes += homaPktHdrSize + IP_HEADER_SIZE +
            UDP_HEADER_SIZE;

    if (numPartialBytes < MIN_ETHERNET_PAYLOAD_BYTES) {
        numPartialBytes = MIN_ETHERNET_PAYLOAD_BYTES;
    }

    uint32_t numPartialBytesOnWire = numPartialBytes + ETHERNET_HDR_SIZE +
            ETHERNET_CRC_SIZE + ETHERNET_PREAMBLE_SIZE + INTER_PKT_GAP;

    bytesOnWire += numPartialBytesOnWire;

    return bytesOnWire;
}

/**
 * Strictly order packets. Orders first based on their priorities, then arrival
 * time at module (eg. queue), then creation times. For packets belong to same
 * sender, then order based on msgId as msgIds are monotonically increasing.
 * Then for packets belong to same message, also order based on index of data
 * bytes in packet.
 */
bool
operator>(const HomaPkt& lhs, const HomaPkt& rhs)
{
    if (lhs.getPriority() > rhs.getPriority() ||
            (lhs.getPriority() == rhs.getPriority() &&
            lhs.getArrivalTime() > rhs.getArrivalTime()) ||

            (lhs.getPriority() == rhs.getPriority() &&
            lhs.getArrivalTime() == rhs.getArrivalTime() &&
            lhs.getCreationTime() > rhs.getCreationTime()) ||

            (lhs.getPriority() == rhs.getPriority() &&
            lhs.getArrivalTime() == rhs.getArrivalTime() &&
            lhs.getCreationTime() == rhs.getCreationTime() &&
            lhs.getSrcAddr() == rhs.getSrcAddr() &&
            lhs.getMsgId() > rhs.getMsgId()) ||
            
            (lhs.getPriority() == rhs.getPriority() &&
            lhs.getArrivalTime() == rhs.getArrivalTime() &&
            lhs.getCreationTime() == rhs.getCreationTime() &&
            lhs.getSrcAddr() == rhs.getSrcAddr() &&
            lhs.getMsgId() == rhs.getMsgId() &&
            lhs.getFirstByte() > rhs.getFirstByte())) {
        // The last two set of conditions are necessary for ordering pkts belong
        // to same message before they are sent to network.
        return true;
    } else {
        return false;
    }
}
