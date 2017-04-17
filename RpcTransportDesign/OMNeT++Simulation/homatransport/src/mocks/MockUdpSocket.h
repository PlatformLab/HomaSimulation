class MockUdpSocket {
  PUBLIC:
    MockUdpSocket(){}
    void setOutputGate(cGate* gate){}
    void bind(uint32_t portNum){}
    void sendTo(cPacket* pkt, inet::L3Address dest, uint32_t destPort)
    {
        delete pkt;
    }
};
