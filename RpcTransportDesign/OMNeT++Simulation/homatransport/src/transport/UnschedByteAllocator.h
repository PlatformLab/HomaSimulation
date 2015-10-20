/*
 * UnschedByteAllocator.h
 *
 *  Created on: Oct 15, 2015
 *      Author: behnamm
 */

#include <map>
#include <unordered_map>
#include "HomaPkt.h"

#ifndef UNSCHEDBYTEALLOCATOR_H_
#define UNSCHEDBYTEALLOCATOR_H_

class UnschedByteAllocator
{
  public:
    explicit UnschedByteAllocator(uint32_t defaultReqBytes,
            uint32_t defaultUnschedBytes);
    ~UnschedByteAllocator();
    uint32_t getReqDataBytes(uint32_t rxAddr, uint32_t msgSize);
    uint32_t getUnschedBytes(uint32_t rxAddr, uint32_t msgSize);
    void updateReqDataBytes(HomaPkt* grantPkt);
    void updateUnschedBytes(HomaPkt* grantPkt);

  private:
    void initReqBytes(uint32_t rxAddr); 
    void initUnschedBytes(uint32_t rxAddr); 

  private:
    std::unordered_map<uint32_t, std::map<uint32_t, uint32_t>>
            rxAddrUnschedbyteMap;
    std::unordered_map<uint32_t, std::map<uint32_t, uint32_t>>
            rxAddrReqbyteMap;
    uint32_t defaultReqBytes;
    uint32_t defaultUnschedBytes;
};
#endif /* UNSCHEDBYTEALLOCATOR_H_ */
