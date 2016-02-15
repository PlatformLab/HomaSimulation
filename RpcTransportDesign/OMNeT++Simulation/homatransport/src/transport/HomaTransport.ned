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
        int grantMaxBytes @unit(byte) = default(1472B);
        int maxOutstandingRecvBytes @unit(byte) = default(15000B);
        int defaultReqBytes @unit(byte) = default(0B);
        int defaultUnschedBytes @unit(byte) = default(9700B);
        int maxRtt @unit(us) = default(15us);
        bool isRoundRobinScheduler = default(false);
        int localPort = default(-1);
        int destPort;
        bool useUnschRateInScheduler = default(false);

        // number of available priorities
        int prioLevels;
        int schedPrioLevels;
        int prioResolverPrioLevels;

        // Could be any of PrioPaceMode values defined in TrafficPacer.h
        string schedPrioAssignMode = default("NO_OVER_COMMIT");
        // Coulde be any of PrioResolutionMode values in PriorityResolver.h
        string unschedPrioResolutionMode;
        // Specifies that only first cbfCapMsgSize bytes of a message must
        // be used in computing the cbf function. This value is supposed to be uint32_t
        // type, but since NED don't support 'unsigned int',
        // we enter integers in the string form.
        string cbfCapMsgSize = default("0xFFFFFFFF");

        // Only for simulation purpose, we allow the transport to be aware of
        // workload type. The value must be similar to WorkloadSynthesizer
        // workloadType.
        string workloadType = default("");

        @signal[msgsLeftToSend](type=long);
        @signal[bytesLeftToSend](type=long);
        @signal[totalOutstandingBytes](type=long);
        @signal[outstandingGrantBytes](type=long);
        @singal[homaPktPrio*Signal](type=HomaPkt);

        @statistic[msgsLeftToSend](title="num msgs left to send"; source=msgsLeftToSend; record=stats, histogram; interpolationmode=none);
        @statistic[bytesLeftToSend](title="num total bytes left to send"; source=bytesLeftToSend; record=stats, histogram; interpolationmode=none);
        @statistic[totalOutstandingBytes](title="total bytes in flight"; source=totalOutstandingBytes; record=stats, histogram; interpolationmode=none);
        @statistic[outstandingGrantBytes](title="num total bytes left to send"; source=outstandingGrantBytes; record=stats, histogram; interpolationmode=none);
        @statisticTemplate[pktPrioStats](title="Per pkt priority statistics"; record=stats(homaMsgSize), stats(homaPktBytes), stats(homaUnschedPktBytes); interpolationmode=none);

    gates:
        input appIn[] @labels(AppMessage/down);
        input udpIn @labels(UDPControlInfo/up);
        output appOut[] @labels(AppMessage/up);
        output udpOut @labels(UDPControlInfo/down);
}
