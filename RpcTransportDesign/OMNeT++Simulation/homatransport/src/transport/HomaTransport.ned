//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

package homatransport.transport;
import homatransport.transport.ITransportScheme;

//
// defines a credit based receiver side driven transport protocol
//
simple HomaTransport like ITransportScheme
{
    parameters:
        xml transportConfig = default(xml("<param/>"));
        int nicLinkSpeed @unit(Gbps);
        int rttBytes @unit(byte);
        int grantMaxBytes @unit(byte) = default(1472B);
        int maxOutstandingRecvBytes @unit(byte) = default(15000B);
        int defaultReqBytes @unit(byte) = default(0B);
        int defaultUnschedBytes @unit(byte) = default(9700B);
        bool isRoundRobinScheduler = default(false);
        int localPort = default(-1);
        int destPort;
        bool useUnschRateInScheduler = default(false);

        int prioLevels; // All available prios
        int adaptiveSchedPrioLevels; // Sched prio levels used for dynamic. scheduling
                                     // This value must be smaller than schedPrioLevels
        int prioResolverPrioLevels;  // All available prio levels available for priority
                                     // resolver
        int numSendersToKeepGranted; // total number of most preffered senders that
                                     // receiver concurrently sends grants for.

        // priod intervals at which the transport fires signals that are
        // collected by GlobalSignalListener.
        double signalEmitPeriod @unit(s) = default(0.001s);

        // If true, network traffic bytes from grant packets will be accounted in
        // when computing CBF and unscheduled priority cutoff sizes.
        bool accountForGrantTraffic = default(true);

        // Coulde be any of PrioResolutionMode values in PriorityResolver.h
        string unschedPrioResolutionMode;

        // If unschedPrioResolutionMode is set to EXPLICIT,
        // then this string defines the priority cutoff points of unsched
        // bytes for the remaining message sizes. Example would be "100 1500 9000"
        string explicitUnschedPrioCutoff = default("");

        // Specifies for the unsched prio level, how many times prio p_i will be
        // used comparing to p_i-1 (p_i is higher prio). Cutoff weight are used
        // in PrioResolver class.
        double unschedPrioUsageWeight = default(1.0);

        // The kind of logic transport uses at sender for sending messages and pkts
        string senderScheme = default("OBSERVE_PKT_PRIOS");

        // Specifies that only first cbfCapMsgSize bytes of a message must
        // be used in computing the cbf function. This value is supposed to be uint32_t
        // type, but since NED don't support 'unsigned int',
        // we enter integers in the string form.
        string cbfCapMsgSize = default("0x100000000");

        // This value is in bytes and determines that this number of last scheduled
        // bytes of the message will be send at priority equal to unscheduled
        // priorites. The default is 0 bytes.
        string boostTailBytesPrio = default("0x0");

        // If receiver inbound link is idle for longer than "linkCheckBytes /
        // linkSpeed", while there are senders waiting for grants, we consider
        // receiver bw is wasted. -1 is reserved to disable bw-waste detection
        //with this mechanism.
        int linkCheckBytes @unit(byte) = default(-1B);

        // Only for simulation purpose, we allow the transport to be aware of
        // workload type. The value must be similar to WorkloadSynthesizer
        // workloadType.
        string workloadType = default("");

        @signal[msgsLeftToSend](type=long);
        @signal[stabilitySignal](type=unsigned long);
        @signal[bytesLeftToSend](type=long);
        @signal[totalOutstandingBytes](type=long);
        @signal[totalOutstandingBytes](type=long);
        @singal[homaPktPrio*Signal](type=HomaPkt);
        @signal[rxActiveTime](type=simtime_t);
        @signal[higherRxSelfWasteTime](type=simtime_t);
        @signal[lowerRxSelfWasteTime](type=simtime_t);
        @signal[rxActiveBytes](type=long);
        @signal[oversubscriptionTime](type=simtime_t);
        @signal[oversubscriptionBytes](type=long);
        @signal[sxActiveTime](type=simtime_t);
        @signal[sxActiveBytes](type=long);
        @signal[sxSchedPktDelay](type=simtime_t);
        @signal[sxUnschedPktDelay](type=simtime_t);
        @signal[activeSchedMesgs];

        @statistic[msgsLeftToSend](title="num msgs left to send"; source=msgsLeftToSend; record=stats, histogram; interpolationmode=none);
        @statistic[bytesLeftToSend](title="num total bytes left to send"; source=bytesLeftToSend; record=stats, histogram; interpolationmode=none);
        @statistic[bytesNeedGrant](title="num total sched bytes left to send"; source=bytesNeedGrant; record=stats, histogram; interpolationmode=none);
        @statistic[totalOutstandingBytes](title="total bytes in flight"; source=totalOutstandingBytes; record=stats, histogram; interpolationmode=none);
        @statistic[outstandingGrantBytes](title="num total bytes left to send"; source=outstandingGrantBytes; record=stats, histogram; interpolationmode=none);
        @statisticTemplate[pktPrioStats](title="Per pkt priority statistics"; record=stats(homaMsgSize), stats(homaPktBytes), stats(homaUnschedPktBytes), stats(homaGrantPktBytes); interpolationmode=none);
        @statistic[rxActiveTime](title="Time periods receiver expects to get bytes"; source=rxActiveTime; record=stats, histogram; interpolationmode=none);
        @statistic[rxActiveBytes](title="Bytes received during each active period"; source=rxActiveBytes; record=stats, histogram; interpolationmode=none);
        @statistic[oversubscriptionTime](title="Time periods receiver has more senders than it grants"; source=oversubscriptionTime; record=stats, histogram; interpolationmode=none);
        @statistic[oversubscriptionBytes](title="Bytes received during each oversubscription time"; source=oversubscriptionBytes; record=stats, histogram; interpolationmode=none);
        @statistic[sxActiveTime](title="Time periods sender has bytes to send"; source=sxActiveTime; record=stats, histogram; interpolationmode=none);
        @statistic[sxActiveBytes](title="Bytes sent during each active period"; source=sxActiveBytes; record=stats, histogram; interpolationmode=none);
        @statistic[sxSchedPktDelay](title="Sender delays for sched pkts"; source=sxSchedPktDelay; record=stats, histogram; interpolationmode=none);
        @statistic[sxUnschedPktDelay](title="Sender delays for unsched pkts"; source=sxUnschedPktDelay; record=stats, histogram; interpolationmode=none);

    gates:
        input appIn[] @labels(AppMessage/down);
        input udpIn @labels(UDPControlInfo/up);
        output appOut[] @labels(AppMessage/up);
        output udpOut @labels(UDPControlInfo/down);
}
