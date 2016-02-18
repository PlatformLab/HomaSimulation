/*
 * UnschedByteAllocator.cc
 *
 *  Created on: Oct 15, 2015
 *      Author: behnamm
 */

#include <cmath>
#include "UnschedByteAllocator.h"

UnschedByteAllocator::UnschedByteAllocator(HomaConfigDepot* homaConfig)
    : rxAddrUnschedbyteMap()
    , rxAddrReqbyteMap()
    , homaConfig(homaConfig)

{
    HomaPkt reqPkt = HomaPkt();
    reqPkt.setPktType(PktType::REQUEST);
    maxReqPktDataBytes = MAX_ETHERNET_PAYLOAD_BYTES -
        IP_HEADER_SIZE - UDP_HEADER_SIZE - reqPkt.headerSize();

    HomaPkt unschedPkt = HomaPkt();
    unschedPkt.setPktType(PktType::UNSCHED_DATA);
    maxUnschedPktDataBytes = MAX_ETHERNET_PAYLOAD_BYTES -
        IP_HEADER_SIZE - UDP_HEADER_SIZE - unschedPkt.headerSize();
}

UnschedByteAllocator::~UnschedByteAllocator()
{}

void
UnschedByteAllocator::initReqBytes(uint32_t rxAddr)
{
    if (homaConfig->defaultReqBytes > maxReqPktDataBytes) {
        throw cRuntimeError("Config Param defaultReqBytes set: %u, is larger than"
            " max possible bytes: %u", homaConfig->defaultReqBytes,
            maxReqPktDataBytes);
    }
    rxAddrReqbyteMap[rxAddr].clear();
    rxAddrReqbyteMap[rxAddr] = {{UINT32_MAX, homaConfig->defaultReqBytes}};
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
    rxAddrUnschedbyteMap[rxAddr] = {{UINT32_MAX,
        homaConfig->defaultUnschedBytes}};
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

/**
 * Return value must be a a vector of at least size 1.
 */
std::vector<uint16_t>
UnschedByteAllocator::getReqUnschedDataPkts(uint32_t rxAddr, uint32_t msgSize)
{
    uint32_t reqData = getReqDataBytes(rxAddr, msgSize);
    std::vector<uint16_t> reqUnschedPktsData = {};
    reqUnschedPktsData.push_back(downCast<uint16_t>(reqData));
    uint32_t unschedData = getUnschedBytes(rxAddr, msgSize);
    while (unschedData > 0) {
        uint32_t unschedInPkt = std::min(unschedData, maxUnschedPktDataBytes);
        reqUnschedPktsData.push_back(downCast<uint16_t>(unschedInPkt));
        unschedData -= unschedInPkt;
    }
    return reqUnschedPktsData;
}


void
UnschedByteAllocator::updateReqDataBytes(HomaPkt* grantPkt)
{}

void
UnschedByteAllocator::updateUnschedBytes(HomaPkt* grantPkt)
{}
