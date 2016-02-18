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

#ifndef __HOMA_CONFIG_DEPOT_H_
#define __HOMA_CONFIG_DEPOT_H_

#include "omnetpp.h"
#include "common/Minimal.h"

// This class is the ultimate place for storing all config parameters. All other
// classes will get a pointer to this class and can read params from this
class HomaConfigDepot {
  PUBLIC:
    HomaConfigDepot(cComponent* ownerTransport);
    ~HomaConfigDepot(){}

  PUBLIC:
    cComponent* ownerTransport;

    // Contains the parameters defined in the xml file config.xml in under
    // "/topologyConfig/hostConfig[@id=HostId]/transportConfig" section.
    cXMLElement* xmlConfig;

    // NIC link speed (in Gb/s) connected to this host. This parameter will be
    // read from the omnetpp.ini config file.
    int nicLinkSpeed;

    // This parameter is read from the omnetpp.ini config file and provides an
    // upper bound on the total allowed outstanding bytes. It is necessary (BUT
    // NOT ENOUGH) for the rxScheduler to check that the total outstanding bytes
    // is smaller than this value every time a new grant is to be sent.
    int maxOutstandingRecvBytes;

    // udp ports assigned to this transprt
    int localPort;
    int destPort;

    // Maximum possible data bytes allowed in grant
    uint32_t grantMaxBytes;

    // Total number of available priorities
    uint16_t allPrio;

    // Total number of available priorities for the scheduling scheme.
    uint16_t schedPrioLevels;

    // Total number of priorities that PrioResolver would use to resolve
    // priorities.
    uint16_t prioResolverPrioLevels;

    // Specifies which priority resolution mode should be used for scheduled
    // packets. The pace modes are defined in TrafficPacer.
    const char* schedPrioAssignMode;

    // Specifies which priority resolution mode should be used for unscheduled
    // packets. Resolution modes are defined in PrioResolver class.
    const char* unschedPrioResolutionMode;

    // Specifies the scheduler type. True, means round robin scheduler and false
    // is for SRBF scheduler.
    bool isRoundRobinScheduler;

    // Specifies that only first cbfCapMsgSize bytes of a message must be used
    // in computing the cbf function.
    uint32_t cbfCapMsgSize;

    // Number data bytes to be packed in request packet.
    uint32_t defaultReqBytes;

    // Maximum number of unscheduled data bytes that will be sent in unsched.
    // packets except the data bytes in request packet.
    uint32_t defaultUnschedBytes;

    // True means that scheduler would use
    // (1-avgUnscheduledRate)*maxOutstandingRecvBytes as the cap for number of
    // scheduled bytes allowed outstanding.
    bool useUnschRateInScheduler;

    // Only for simulation purpose, we allow the transport to be aware of
    // workload type. The value must be similar to WorkloadSynthesizer
    // workloadType.
    const char* workloadType;
};
#endif
