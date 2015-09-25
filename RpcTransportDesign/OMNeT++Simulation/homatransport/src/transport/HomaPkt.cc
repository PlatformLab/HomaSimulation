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
            size += sizeof(getReqFields().msgByteLen) + sizeof(getReqFields().numReqBytes)
                    + sizeof(getReqFields().totalUnschedBytes);
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

