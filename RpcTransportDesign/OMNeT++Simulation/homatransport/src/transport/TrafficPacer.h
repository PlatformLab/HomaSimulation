/*
 * TrafficPacer.h
 *
 *  Created on: Sep 18, 2015
 *      Author: neverhood
 */

#ifndef TRAFFICPACER_H_
#define TRAFFICPACER_H_

#include <omnetpp.h>
#include <vector>
#include <map>
#include <utility>
#include "common/Minimal.h"
#include "transport/HomaPkt.h"
#include "transport/HomaTransport.h"
#include "transport/PriorityResolver.h"

/**
 * This module is an essential part of receiver driven congestion control
 * mechansim of HomaTransport. Providing the highest priority outstanding
 * message, this module determines whether grant can be sent for that message
 * and what the priority the grant must be sent.
 */

class TrafficPacer
{
  PUBLIC:
    typedef HomaTransport::InboundMessage InboundMessage;
    typedef HomaTransport::ReceiveScheduler::UnschedRateComputer
        UnschedRateComputer;

    enum PrioPaceMode {
        FIXED = 0,
        ADAPTIVE_LOWEST_PRIO_POSSIBLE,
        STATIC_FROM_CBF,
        STATIC_FROM_CDF,
        SIMULATED_SRBF,
        SMF_CBF_BASED, //shortest msg first based on cbf
        HEAD_TAIL_BYTES_FIRST_EQUAL_BYTES,
        HEAD_TAIL_BYTES_FIRST_EXP_BYTES,
        HEAD_TAIL_BYTES_FIRST_EQUAL_COUNTS,
        HEAD_TAIL_BYTES_FIRST_EXP_COUNTS,
        INVALIDE_MODE //Always the last one
    };

    TrafficPacer(PriorityResolver* prioRes, UnschedRateComputer* unschRateComp,
        HomaConfigDepot* homaConfig);
    ~TrafficPacer();

  PRIVATE:
    class InbndMesgCompare {
      PUBLIC:
        InbndMesgCompare(){}

        /**
         * This function returns a total order on the inbound messages. It
         * returns false only if the two arguments rhs and lhs are infact the
         * same message. Otherwise, it will impose a total order among the
         * inbound messages first based on the remaining bytes to grant for the
         * messages, then based on the actual messages sizes. If the ties can't
         * be broken using these two factors, then the pointer addresses rhs and
         * lhs will be used to break the ties and strictly order the messages.
         */
        bool operator()(const InboundMessage* lhs, const InboundMessage* rhs)
        {
            if (lhs == rhs) {
                return false;
            }

            if (lhs->srcAddr == rhs->srcAddr &&
                    lhs->msgIdAtSender == rhs->msgIdAtSender) {
                return false;
            }

            return lhs->bytesToGrant < rhs->bytesToGrant ||
                (lhs->bytesToGrant == rhs->bytesToGrant &&
                lhs->msgSize < rhs->msgSize) ||
                (lhs->bytesToGrant == rhs->bytesToGrant &&
                lhs->msgSize == rhs->msgSize && lhs < rhs);
        }
    };
    typedef std::map<InboundMessage*, uint32_t, InbndMesgCompare>
        InbndMsgOrderedMap;
    simtime_t getNextGrantTime(simtime_t currentTime,
        uint32_t grantedPktSizeOnWire);
    PriorityResolver::PrioResolutionMode prioPace2PrioResolution(
        PrioPaceMode prioPaceMode);

    // following functions transform respective prio and index for
    // inflightSchedPerPrio and inflightUnschedPerPrio vectors. The
    // implementation depends on the vector sizes set at construction time. 
    uint16_t prioToSchedInd(uint16_t prio) { return prio; }
    uint16_t prioToUnschedInd(uint16_t prio) { return prio; }
    uint16_t schedIndToPrio(uint16_t ind) { return ind; }
    uint16_t unschedIndToPrio(uint16_t ind) { return ind; }


  PUBLIC:
    void bytesArrived(InboundMessage* inbndMsg, uint32_t arrivedBytes,
        PktType recvPktType, uint16_t prio);
    void unschedPendingBytes(InboundMessage* inbndMsg, uint32_t committedBytes,
        PktType pendingPktType, uint16_t prio);
    HomaPkt* getGrant(simtime_t currentTime, InboundMessage* msgToGrant,
        simtime_t &nextTimeToGrant);
    PrioPaceMode strPrioPaceModeToEnum(const char* prioPaceMode);

    // Only for stats collection and analysis purpose
    inline uint32_t getTotalOutstandingBytes()
    {
        return totalOutstandingBytes;
    }

    inline uint32_t getUnschedOutstandingBytes()
    {
        return unschedInflightBytes;
    }

    inline const std::vector<uint32_t>& getSumInflightUnschedPerPrio()
    {
        return sumInflightUnschedPerPrio;
    }

    inline const std::vector<uint32_t>& getSumInflightSchedPerPrio()
    {
        return sumInflightSchedPerPrio;
    }


  PRIVATE:
    PriorityResolver* prioResolver;
    UnschedRateComputer* unschRateComp;
    HomaConfigDepot* homaConfig;
    simtime_t nextGrantTime;

    // The upper bound on the number of grant data byte carried sent in
    // individual grant packets .
    std::vector<InbndMsgOrderedMap> inflightUnschedPerPrio;
    std::vector<InbndMsgOrderedMap> inflightSchedPerPrio;
    std::vector<uint32_t> sumInflightUnschedPerPrio;
    std::vector<uint32_t> sumInflightSchedPerPrio;
    int totalOutstandingBytes;
    int unschedInflightBytes;
    PrioPaceMode paceMode;
    uint16_t lastGrantPrio;
};
#endif /* TRAFFICPACER_H_ */
