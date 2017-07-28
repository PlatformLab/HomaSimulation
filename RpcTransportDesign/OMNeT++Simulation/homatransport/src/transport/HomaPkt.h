/*
 * HomaPkt.h
 *
 *  Created on: Sep 16, 2015
 *      Author: behnamm
 */

#ifndef HOMAPKT_H_
#define HOMAPKT_H_

#include "transport/HomaPkt_m.h"
#include "common/Minimal.h"
#include "transport/HomaConfigDepot.h"

/**
 * All of the constants packet and header byte sizes for different types of
 * datagrams.
 */
static const uint32_t ETHERNET_PREAMBLE_SIZE = 8;
static const uint32_t ETHERNET_HDR_SIZE = 14;
static const uint32_t MAX_ETHERNET_PAYLOAD_BYTES = 1500;
static const uint32_t MIN_ETHERNET_PAYLOAD_BYTES = 46;
static const uint32_t MIN_ETHERNET_FRAME_SIZE = 64;
static const uint32_t IP_HEADER_SIZE = 20;
static const uint32_t UDP_HEADER_SIZE = 8;
static const uint32_t ETHERNET_CRC_SIZE = 4;
static const uint32_t INTER_PKT_GAP = 12;

class HomaTransport;
class HomaPkt : public HomaPkt_Base
{
  PUBLIC:
    // Points to the transport that owned this packet last time this packet was
    // in an end host. This is only used for statistics collection.
    // This value is assigned when the packet is constructed and is updated by
    // homatransport as soon as it arrives at the transport.
    HomaTransport* ownerTransport;

  PRIVATE:
    void copy(const HomaPkt& other);

  PUBLIC:
    HomaPkt(HomaTransport* ownerTransport = NULL, const char *name=NULL,
        int kind=0);
    HomaPkt(const HomaPkt& other);
    HomaPkt& operator=(const HomaPkt& other);
    virtual HomaPkt *dup() const;
    uint32_t getFirstByte() const;
    friend bool operator>(const HomaPkt &lhs, const HomaPkt &rhs);

    /**
     * A utility predicate for creating PriorityQueues of HomaPkt instances
     * based on priority numbers.
     */
    class HomaPktSorter {
      PUBLIC:
        HomaPktSorter(){}

        /**
         * Predicate functor operator () for comparison.
         *
         * \param pkt1
         *      first pkt for priority comparison
         * \param pkt2
         *      second pkt for priority comparison
         * \return
         *      true if pkt1 compared greater than pkt2.
         */
        bool operator()(const HomaPkt* pkt1, const HomaPkt* pkt2)
        {
            return *pkt1 > *pkt2;
        }
    };

    // ADD CODE HERE to redefine and implement pure virtual functions from
    // HomaPkt_Base

    /**
     * returns the header size of this packet.
     */
    uint32_t headerSize();

    /**
     * returns number of data bytes carried in the packet (if any)
     */
    uint32_t getDataBytes();

    /**
     * This function checks if there is a HomaPkt packet encapsulated in the
     * messages and returns it. Returns null if no HomaPkt is encapsulated.
     */
    static cPacket* searchEncapHomaPkt(cPacket* msg);

    /**
     * This function compares priority of two HomaPkt.
     */
    static int comparePrios(cObject* obj1, cObject* obj2);

    /**
     * This function compares priority and msg sizes of two Unsched. HomaPkt.
     */
    static int compareSizeAndPrios(cObject* obj1, cObject* obj2);

    static uint32_t getBytesOnWire(uint32_t numDataBytes, PktType homaPktType);
    static uint32_t maxEthFrameSize() {
        return ETHERNET_PREAMBLE_SIZE + ETHERNET_HDR_SIZE +
            MAX_ETHERNET_PAYLOAD_BYTES + ETHERNET_CRC_SIZE + INTER_PKT_GAP;
    }
};


#endif /* HOMAPKT_H_ */
