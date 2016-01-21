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
#include "transport/HomaPkt.h"
#include "transport/HomaTransport.h"
#include "transport/PriorityResolver.h"

class TrafficPacer
{
  public:
    typedef HomaTransport::InboundMessage InboundMessage;
    typedef HomaTransport::ReceiveScheduler::UnschedRateComputer
        UnschedRateComputer;

    enum PrioPaceMode {
        FIXED = 0,
        ADAPTIVE_LOWEST_PRIO_POSSIBLE,
        STATIC_FROM_CBF,
        STATIC_FROM_CDF,
        INVALIDE_MODE //Always the last one
    };

    TrafficPacer(PriorityResolver* prioRes, UnschedRateComputer* unschRateComp,
        double nominalLinkSpeed, uint16_t allPrio, uint16_t schedPrio,
        uint32_t grantMaxBytes, uint32_t maxAllowedInFlightBytes = 10000,
        const char* prioPaceMode = "FIXED");
    ~TrafficPacer();
    static constexpr double ACTUAL_TO_NOMINAL_RATE_RATIO = 1.0;

  private:
    class InbndMesgCompare {
      public:
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

  public:
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

  private:
    PriorityResolver* prioResolver;
    UnschedRateComputer* unschRateComp;
    double actualLinkSpeed;
    simtime_t nextGrantTime;
    uint32_t maxAllowedInFlightBytes;

    // The upper bound on the number of grant data byte carried sent in
    // individual grant packets .
    uint32_t grantMaxBytes;
    std::vector<InbndMsgOrderedMap> inflightUnschedPerPrio;
    std::vector<InbndMsgOrderedMap> inflightSchedPerPrio;
    int totalOutstandingBytes;
    int unschedInflightBytes;
    PrioPaceMode paceMode;
    uint16_t allPrio;
    uint16_t schedPrio;
};
#endif /* TRAFFICPACER_H_ */
