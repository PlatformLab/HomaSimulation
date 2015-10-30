/*
 * HomaPkt.cc
 *
 *  Created on: Sep 16, 2015
 *      Author: behnamm
 */


#include "HomaPkt.h"

Register_Class(HomaPkt);



HomaPkt::HomaPkt(const char *name, int kind)
    : HomaPkt_Base(name,kind)
{}

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
{}

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
                    sizeof(getReqFields().totalUnschedBytes);
            break;

        case PktType::UNSCHED_DATA:
            size += (sizeof(getUnschedDataFields().msgByteLen) + 
                    sizeof(getReqFields().numReqBytes) +
                    sizeof(getReqFields().totalUnschedBytes) +
                    sizeof(getUnschedDataFields().firstByte) + 
                    sizeof(getUnschedDataFields().lastByte));
            break;
        case PktType::GRANT:
            size += sizeof(getGrantFields().grantBytes);
            
        case PktType::SCHED_DATA:
            size += (sizeof(getSchedDataFields().firstByte) + 
                    sizeof(getSchedDataFields().lastByte));
            break;
        default:
            break;
    }
    return size;
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
