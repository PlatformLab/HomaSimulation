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


#include "MinimalTransport.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/common/InterfaceEntry.h"
#include "inet/networklayer/common/InterfaceTable.h"
#include "inet/networklayer/ipv4/IPv4InterfaceData.h"

Define_Module(MinimalTransport);

/**
* Register stats collection signals.
*/
simsignal_t MinimalTransport::msgsLeftToSendSignal =
        registerSignal("msgsLeftToSend");
simsignal_t MinimalTransport::bytesLeftToSendSignal =
        registerSignal("bytesLeftToSend");

/**
 * Construct a minimal transport simple module
 */
MinimalTransport::MinimalTransport()
    : bytesLeftToSend(0)
    , msgsLeftToSend(0)
    , socket()
    , selfMesg(NULL)
    , localPort(-1)
    , destPort(-1)
    , nicLinkSpeed(0)
    , xmlConfig(NULL)
    , localAddr()
    , oracleScheduler(NULL)
    , maxDataBytesPerPkt(1400)
{}

/**
 * Destructor for the transport.
 */
MinimalTransport::~MinimalTransport()
{
    cancelAndDelete(selfMesg);
}

/**
 * Method is call be omnet simulation engine at the topology setup phase.
 * All initialzation related affairs must be handled in this method. 
 */
void
MinimalTransport::initialize()
{
    HomaPkt schedPkt;
    schedPkt.setPktType(PktType::SCHED_DATA);
    maxDataBytesPerPkt = MAX_ETHERNET_PAYLOAD_BYTES - IP_HEADER_SIZE -
        UDP_HEADER_SIZE - schedPkt.headerSize();

    // Read module parameters
    localPort = par("localPort");
    destPort = par("destPort");
    nicLinkSpeed = par("nicLinkSpeed");
    xmlConfig = par("transportConfig").xmlValue();

    // Create transmit timer object
    selfMesg = new cMessage("TransmitTimer");
    selfMesg->setKind(SelfMesgKind::START);
    scheduleAt(simTime(), selfMesg);

}

/**
 * This method is called by the omnet simulation engine at the end of
 * simulation before calling destructors. Can be used for cleaning up and
 * dumping stats and log files.
 */
void
MinimalTransport::finish()
{}

/**
 * Timer and message handler method.
 *
 * \param mesg
 *      Can be the timer selfMesg or any message that has arrived on gates of
 *      this module.
 */
void
MinimalTransport::handleMessage(cMessage* mesg)
{
    if (mesg->isSelfMessage()) {
        switch(mesg->getKind()) {
            case SelfMesgKind::START:
                processStart();
                break;
            case SelfMesgKind::TRANSMITTING:
                processTransmitPkt();
                break;
            default:
                throw cRuntimeError("Unknown self message type %d",
                    mesg->getKind());
        }
    } else {
        if (mesg->arrivedOn("rpcHandlerIn")) {
            processSendRpc(check_and_cast<Rpc*>(mesg));
        } else if (mesg->arrivedOn("udpIn")) {
            processRcvdPkt(check_and_cast<HomaPkt*>(mesg));
        } else {
            throw cRuntimeError("Unknown message has been received.");
        }
    }
}

/**
 * This method is called after initialization. Handles the latest intialization
 * stage that require cross-dependencies from other modules. Also starts up the
 * transport activites.
 */
void
MinimalTransport::processStart()
{
    // set localAddr. The assumption here is that each host has only one
    // non-loopback interface and the IP of that interface is localAddr.
    inet::InterfaceTable* ifaceTable =
            check_and_cast<inet::InterfaceTable*>(
            getModuleByPath(par("interfaceTableModule").stringValue()));
    inet::InterfaceEntry* srcIface = NULL;
    inet::IPv4InterfaceData* srcIPv4Data = NULL;
    for (int i=0; i < ifaceTable->getNumInterfaces(); i++) {
        if ((srcIface = ifaceTable->getInterface(i)) &&
                !srcIface->isLoopback() &&
                (srcIPv4Data = srcIface->ipv4Data())) {
            break;
        }
    }
    if (!srcIface) {
        throw cRuntimeError("Can't find a valid interface on the host");
    } else if (!srcIPv4Data) {
        throw cRuntimeError("Can't find an interface with IPv4 address");
    }
    localAddr = inet::L3Address(srcIPv4Data->getIPAddress());

    // Setup udp socket to send packets through it
    socket.setOutputGate(gate("udpOut"));
    socket.bind(localPort);

    // Register this transort to the OracleGreedySRPTScheduler.
    oracleScheduler = check_and_cast<OracleGreedySRPTScheduler*>(
        getModuleByPath("oracleSRPTScheduler"));
    oracleScheduler->subscribeEndhostTransport(this, localAddr);
    selfMesg->setKind(SelfMesgKind::TRANSMITTING);
}

/**
 * Handles transmission of messages that arrive from application layer.
 *
 * \param mesg
 *      Outbound message from application that needs to be transmitted over the
 *      network to a different destination.
 */
void
MinimalTransport::processSendRpc(Rpc* mesg)
{
    bytesLeftToSend += mesg->getByteLength();
    msgsLeftToSend++;
    emit(msgsLeftToSendSignal, msgsLeftToSend);
    emit(bytesLeftToSendSignal, bytesLeftToSend);
    // For every new message that arrives from application, the message needs to
    // be scheduled for transmission by the centralized oracle scheduler.
    oracleScheduler->scheduleNewMesg(mesg);
    delete mesg;
}

/**
 * If there is a message scheduled for transmission by this transport, this
 * method prepares a chunk of the message in a packet and send it to the
 * destination. Consecutively, after transmission of each packet, a timer is
 * set for the next transmission for at least one packet time later.
 */
void
MinimalTransport::processTransmitPkt()
{
    ASSERT(selfMesg->getKind() == SelfMesgKind::TRANSMITTING);
    MesgChunk* sxChunk =
        oracleScheduler->getNextChunkToSend(this, maxDataBytesPerPkt);
    if (!sxChunk)
        return;
    bytesLeftToSend -= sxChunk->chunkLen;
    sxChunk->lastChunk ? msgsLeftToSend-- : msgsLeftToSend;

    // create a data pkt and send out
    HomaPkt* dataPkt = new HomaPkt(NULL);
    dataPkt->setPktType(PktType::SCHED_DATA);
    dataPkt->setSrcAddr(sxChunk->srcAddr);
    dataPkt->setDestAddr(sxChunk->destAddr);
    dataPkt->setMsgId(sxChunk->msgId);
    dataPkt->setPriority(0);
    SchedDataFields dataFields;
    dataFields.firstByte = sxChunk->offsetByte;
    dataFields.lastByte = dataFields.firstByte + sxChunk->chunkLen - 1;
    dataPkt->setSchedDataFields(dataFields);
    dataPkt->setByteLength(sxChunk->chunkLen + dataPkt->headerSize());
    socket.sendTo(dataPkt, dataPkt->getDestAddr(), destPort);

    // schedule next pkt transmission
    uint32_t pktSizeOnWire = HomaPkt::getBytesOnWire(
        sxChunk->chunkLen, (PktType)dataPkt->getPktType());
    simtime_t nextTransmitTime = SimTime(1e-9 * (pktSizeOnWire * 8.0 /
        nicLinkSpeed)) + simTime();
    if (selfMesg->isScheduled())
        cancelEvent(selfMesg);
    scheduleAt(nextTransmitTime, selfMesg); 
    delete sxChunk;
}

/**
 * For every new packet that's been received, calling this function appends it
 * to the corresponding inbound message. Moreover, when the inbound message is
 * complete the message will be delivered to the application.
 *
 * \param rcvdPkt
 *      The data packet that has arrived from the network and needs to be
 *      appended to the inbound message.
 */
void
MinimalTransport::processRcvdPkt(HomaPkt* rcvdPkt)
{
    MesgChunk msgChunk;
    msgChunk.msgId = rcvdPkt->getMsgId();
    msgChunk.srcAddr = rcvdPkt->getSrcAddr();
    msgChunk.destAddr = rcvdPkt->getDestAddr();
    msgChunk.offsetByte = rcvdPkt->getSchedDataFields().firstByte;
    msgChunk.chunkLen = rcvdPkt->getDataBytes();
    Rpc* mesgToApp = oracleScheduler->appendRecvdChunk(&msgChunk);
    if (mesgToApp) {
        uint32_t bytesOnWire = HomaPkt::getBytesOnWire(
            mesgToApp->getByteLength(), PktType::SCHED_DATA);
        mesgToApp->setMsgBytesOnWire(bytesOnWire);
        send(mesgToApp, "rpcHandlerOut");
    }
    delete rcvdPkt;
}

/**
 * This method must be called every time the transmission schedules are changed.
 * The oracle scheduler calles this method to notify this transport of newly
 * scheduled message ready for transmission.
 */
void
MinimalTransport::refreshSchedule()
{
    if (selfMesg->isScheduled()) {
        return;
    }
    ASSERT(selfMesg->getKind() == SelfMesgKind::TRANSMITTING);
    scheduleAt(simTime(), selfMesg);
}
