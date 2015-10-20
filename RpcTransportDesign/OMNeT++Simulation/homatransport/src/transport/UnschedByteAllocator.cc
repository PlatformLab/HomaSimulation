/*
 * UnschedByteAllocator.cc
 *
 *  Created on: Oct 15, 2015
 *      Author: behnamm
 */

#include <cmath>
#include "UnschedByteAllocator.h"

UnschedByteAllocator::UnschedByteAllocator(uint32_t defaultReqBytes,
        uint32_t defaultUnschedBytes)
    : rxAddrUnschedbyteMap()
    , rxAddrReqbyteMap()
    , defaultReqBytes(defaultReqBytes)
    , defaultUnschedBytes(defaultUnschedBytes)

{}

UnschedByteAllocator::~UnschedByteAllocator()
{}

void
UnschedByteAllocator::initReqBytes(uint32_t rxAddr)
{
    rxAddrReqbyteMap[rxAddr].clear();  
    rxAddrReqbyteMap[rxAddr] = {{UINT32_MAX, defaultReqBytes}};
}

uint32_t
UnschedByteAllocator::getReqDataBytes(uint32_t rxAddr, uint32_t msgSize)
{
    auto reqBytesMap = rxAddrReqbyteMap.find(rxAddr);
    if (reqBytesMap == rxAddrReqbyteMap.end()) {
        initReqBytes(rxAddr);
        reqBytesMap = rxAddrReqbyteMap.find(rxAddr);
    }
    return std::min(reqBytesMap->second.lower_bound(msgSize)->second, msgSize);
}

void
UnschedByteAllocator::initUnschedBytes(uint32_t rxAddr)
{
    rxAddrUnschedbyteMap[rxAddr].clear();
    rxAddrUnschedbyteMap[rxAddr] = {{UINT32_MAX, defaultUnschedBytes}};
}

uint32_t
UnschedByteAllocator::getUnschedBytes(uint32_t rxAddr, uint32_t msgSize)
{
    auto unschedBytesMap = rxAddrUnschedbyteMap.find(rxAddr);
    if (unschedBytesMap == rxAddrUnschedbyteMap.end()) {
        initUnschedBytes(rxAddr);
        unschedBytesMap = rxAddrUnschedbyteMap.find(rxAddr);
    }
    
    uint32_t reqDataBytes = getReqDataBytes(rxAddr, msgSize);
    if (msgSize <= reqDataBytes) {
        return 0;
    }
    return std::min(msgSize - reqDataBytes,
                unschedBytesMap->second.lower_bound(msgSize)->second);
}

void
UnschedByteAllocator::updateReqDataBytes(HomaPkt* grantPkt)
{}

void
UnschedByteAllocator::updateUnschedBytes(HomaPkt* grantPkt)
{}
