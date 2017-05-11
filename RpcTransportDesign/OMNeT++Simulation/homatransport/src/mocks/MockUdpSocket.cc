#include "MockUdpSocket.h"

MockUdpSocket::MockUdpSocket()
    : sxPkts()
{}

MockUdpSocket::~MockUdpSocket() {
    for (auto pkt : sxPkts) {
        delete pkt;
    }
}

void
MockUdpSocket::setOutputGate(cGate* gate)
{}

void
MockUdpSocket::bind(uint32_t portNum)
{}

void
MockUdpSocket::sendTo(cPacket* pkt, inet::L3Address dest, uint32_t destPort) {
    sxPkts.push_back(pkt);
}

HomaPkt*
MockUdpSocket::getGrantPkt(inet::L3Address dest, uint64_t mesgId) {
    for (auto it = sxPkts.begin(); it != sxPkts.end(); it++) {
        HomaPkt* pkt = check_and_cast<HomaPkt*>(*it);
        EV << "GetGrantPkt: "<< pkt->getPktType() << ", " << pkt->getMsgId() <<
            ", " << pkt->getDestAddr() << endl;
        if (pkt->getPktType() == PktType::GRANT && pkt->getMsgId() == mesgId &&
                pkt->getDestAddr() == dest) {
            sxPkts.erase(it);
            return pkt;
        }
    }
    return NULL;
}
