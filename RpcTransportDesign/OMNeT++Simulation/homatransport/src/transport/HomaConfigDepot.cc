/* Copyright (c) 2015-2016 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sstream>
#include "HomaConfigDepot.h"


HomaConfigDepot::HomaConfigDepot(cComponent* ownerTransport)
    : ownerTransport(ownerTransport)
{
    localPort = ownerTransport->par("localPort");
    destPort = ownerTransport->par("destPort");
    nicLinkSpeed = ownerTransport->par("nicLinkSpeed");
    rttBytes = ownerTransport->par("rttBytes");
    rtt = SimTime(1e-9 * rttBytes * 8.0 / nicLinkSpeed);
    maxOutstandingRecvBytes =
        (uint32_t)ownerTransport->par("maxOutstandingRecvBytes");
    grantMaxBytes = (uint32_t) ownerTransport->par("grantMaxBytes");
    allPrio = (uint16_t) ownerTransport->par("prioLevels");
    adaptiveSchedPrioLevels =
        (uint16_t) ownerTransport->par("adaptiveSchedPrioLevels");
    numSendersToKeepGranted =
        (uint16_t) ownerTransport->par("numSendersToKeepGranted");
    ASSERT(numSendersToKeepGranted <= adaptiveSchedPrioLevels);
    signalEmitPeriod = (double) ownerTransport->par("signalEmitPeriod");
    accountForGrantTraffic =
        ownerTransport->par("accountForGrantTraffic").boolValue();
    prioResolverPrioLevels =
        (uint16_t) ownerTransport->par("prioResolverPrioLevels");
    unschedPrioResolutionMode =
        ownerTransport->par("unschedPrioResolutionMode");
    unschedPrioUsageWeight=
        (double) ownerTransport->par("unschedPrioUsageWeight");
    senderScheme = ownerTransport->par("senderScheme");
    isRoundRobinScheduler =
        ownerTransport->par("isRoundRobinScheduler").boolValue();
    cbfCapMsgSize = std::stoul(
        ownerTransport->par("cbfCapMsgSize").stdstringValue(), nullptr, 16);
    boostTailBytesPrio = std::stoul(
        ownerTransport->par("boostTailBytesPrio").stdstringValue(),
        nullptr, 16);
    defaultReqBytes = (uint32_t) ownerTransport->par("defaultReqBytes");
    defaultUnschedBytes = (uint32_t) ownerTransport->par("defaultUnschedBytes");
    useUnschRateInScheduler =
        ownerTransport->par("useUnschRateInScheduler").boolValue();
    xmlConfig = ownerTransport->par("transportConfig").xmlValue();
    workloadType = ownerTransport->par("workloadType").stringValue();
    linkCheckBytes = (uint32_t)ownerTransport->par("linkCheckBytes");
    paramToEnum();

    // read in explicitUnschedPrioCutoff
    std::istringstream ss(
        ownerTransport->par("explicitUnschedPrioCutoff").stdstringValue());
    uint32_t cutoffSize;
    while (ss >> cutoffSize) {
        explicitUnschedPrioCutoff.push_back(cutoffSize);
    }
}

void
HomaConfigDepot::paramToEnum()
{
    if (strcmp(senderScheme, "OBSERVE_PKT_PRIOS") == 0) {
        sxScheme = SenderScheme::OBSERVE_PKT_PRIOS;
    } else if (strcmp(senderScheme, "SRBF") == 0) {
        sxScheme = SenderScheme::SRBF;
    } else {
        throw cRuntimeError("Unknown SenderScheme: %s", senderScheme);
    }
}


