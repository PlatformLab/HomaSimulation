#include <vector>
#include <unordered_map>
#include "common/Minimal.h"
#include <omnetpp.h>
#include "inet/networklayer/common/L3Address.h"
#include "transport/HomaPkt.h"

class MockUdpSocket {
  PUBLIC:
    std::vector<cPacket*> sxPkts;

  PUBLIC:
    MockUdpSocket();
    ~MockUdpSocket();
    void setOutputGate(cGate* gate);
    void bind(uint32_t portNum);
    void sendTo(cPacket* pkt, inet::L3Address dest, uint32_t destPort);
    HomaPkt* getGrantPkt(inet::L3Address dest, uint64_t mesgId);
};
