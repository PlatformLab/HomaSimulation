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
HomaPkt::headerSize()
{
    uint32_t size = sizeof(msgId_var);
    switch(getPktType()) {
        case PktType::REQUEST:
            size += sizeof(getReqFields().msgByteLen) +
                    sizeof(getReqFields().numReqBytes) +
                    sizeof(getReqFields().totalUnschedBytes) +
                    sizeof(getReqFields().unschedPrio);
            break;

        case PktType::UNSCHED_DATA:
            size += (sizeof(getUnschedDataFields().msgByteLen) +
                    sizeof(getReqFields().numReqBytes) +
                    sizeof(getReqFields().totalUnschedBytes) +
                    sizeof(getUnschedDataFields().firstByte) +
                    sizeof(getUnschedDataFields().lastByte)) +
                    sizeof(getUnschedDataFields().reqPrio);
            break;
        case PktType::GRANT:
            size += (sizeof(getGrantFields().grantBytes) +
                sizeof(getGrantFields().schedPrio) +
                getGrantFields().reqBytesPrio.size() *
                sizeof(decltype (getGrantFields().reqBytesPrio)::value_type) +
                getGrantFields().unschedBytesPrio.size() *
                sizeof(decltype (getGrantFields().unschedBytesPrio)::value_type)
                );
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
            return this->getReqFields().numReqBytes;
        case PktType::UNSCHED_DATA:
            return this->getUnschedDataFields().lastByte -
                this->getUnschedDataFields().firstByte + 1;
        case PktType::SCHED_DATA:
            return this->getSchedDataFields().lastByte -
                this->getSchedDataFields().firstByte + 1;
        case PktType::GRANT:
            return 0;
        default:
            cRuntimeError("PktType %d not defined", this->getPktType());
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
    uint32_t msgByteLen1 = homaPkt1->getUnschedDataFields().msgByteLen;
    uint32_t msgByteLen2 = homaPkt2->getUnschedDataFields().msgByteLen;
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
 * this function returns the actual bytes transmitted on the wire.
 *
 * NOTE: calling this function with numDataBytes=0 assumes that you are sending
 * a packet with no data on the wire and you want to see how many bytes an empty
 * packet is on the wire.
 */
uint32_t
HomaPkt::getBytesOnWire(uint32_t numDataBytes, PktType homaPktType)
{

    HomaPkt homaPkt = HomaPkt();
    homaPkt.setPktType(homaPktType);
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
