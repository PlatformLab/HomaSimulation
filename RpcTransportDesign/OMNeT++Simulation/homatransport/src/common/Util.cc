/*
 * Util.cc
 *
 *  Created on: Jan 27, 2016
 *      Author: behnamm
 */


#include "Util.h"
#include "transport/HomaTransport.h"


Register_ResultFilter("homaMsgSize", HomaMsgSizeFilter);
Register_ResultFilter("homaPktBytes", HomaPktBytesFilter);
Register_ResultFilter("homaUnschedPktBytes", HomaUnschedPktBytesFilter);
Register_ResultFilter("homaGrantPktBytes", HomaGrantPktBytesFilter);

void
HomaMsgSizeFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t,
    cObject *object)
{
    HomaPkt* homaPkt = check_and_cast<HomaPkt*>(object);
    uint32_t msgSize;
    PktType pktType = (PktType)homaPkt->getPktType();
    switch (pktType) {
        case PktType::REQUEST:
        case PktType::UNSCHED_DATA:
            msgSize = homaPkt->getUnschedFields().msgByteLen;
            break;
        case PktType::SCHED_DATA:
        case PktType::GRANT: {
            HomaTransport::InboundMessage* inbndMsg;
            HomaTransport::OutboundMessage* outMsg;
            HomaTransport* transport = homaPkt->ownerTransport;
            if ((inbndMsg =
                    transport->rxScheduler.lookupInboundMesg(homaPkt))) {
                msgSize = inbndMsg->getMsgSize();
            } else {
                outMsg = &(transport->sxController.getOutboundMsgMap()->at(
                    homaPkt->getMsgId()));
                ASSERT((transport->getLocalAddr() == homaPkt->getDestAddr()
                    && pktType == PktType::GRANT) ||
                    (transport->getLocalAddr() == homaPkt->getSrcAddr()
                    && pktType == PktType::SCHED_DATA));
                msgSize = outMsg->getMsgSize();
            }
            break;
        }
        default:
            throw cRuntimeError("PktType %d is not known!", pktType);

    }
    fire(this,  t , (double)msgSize);
}

void
HomaPktBytesFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t,
        cObject *object)
{
    HomaPkt* homaPkt = check_and_cast<HomaPkt*>(object);
    fire(this, t, (double)HomaPkt::getBytesOnWire(homaPkt->getDataBytes(),
        (PktType)homaPkt->getPktType()));
}

void
HomaUnschedPktBytesFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t,
        cObject *object)
{
    HomaPkt* homaPkt = check_and_cast<HomaPkt*>(object);
    switch (homaPkt->getPktType()) {
        case PktType::REQUEST:
        case PktType::UNSCHED_DATA:
            fire(this, t, (double)HomaPkt::getBytesOnWire(
                homaPkt->getDataBytes(), (PktType)homaPkt->getPktType()));
            return;
        default:
            return;
    }

}

void
HomaGrantPktBytesFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t,
        cObject *object)
{
    HomaPkt* homaPkt = check_and_cast<HomaPkt*>(object);
    switch (homaPkt->getPktType()) {
        case PktType::GRANT:
            fire(this, t, (double)HomaPkt::getBytesOnWire(
                homaPkt->getDataBytes(), (PktType)homaPkt->getPktType()));
            return;
        default:
            return;
    }
}
