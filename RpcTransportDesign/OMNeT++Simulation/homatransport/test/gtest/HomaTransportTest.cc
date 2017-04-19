#include <gtest/gtest.h>
#include "TestUtil.h"
#include "common/Minimal.h"
#include "transport/HomaTransport.h"
#include "TestMain.h"


class HomaTransportTest : public ::testing::Test {
  public:
    HomaTransport* transport;
    MockUdpSocket* socket;
    HomaTransport::SendController* sxController;
    HomaTransport::ReceiveScheduler* rxScheduler;
    UnschedByteAllocator* unschedByteAllocator;
    HomaConfigDepot* homaConfig;
    HomaTransport::ReceiveScheduler::SchedSenders* schedSenders;
    inet::L3Address localAddr;
    HomaTransportTest()
    {
        cModuleType* transType =
            cModuleType::get("homatransport.transport.HomaTransport");

        /***********************
        transport = (HomaTransport*)transType->create("transport", NULL);
        transport->finalizeParameters();
        transport->buildInside();
        transport->scheduleStart(simTime());
        transport->callInitialize();
        ************************/
        //std::cout << "Creating Homa instance." << std::endl;
        transport = (HomaTransport*)transType->createScheduleInit(
            "transport", netModule);
        localAddr = inet::L3Address("10.0.0.1");
        sxController = &transport->sxController;
        rxScheduler = &transport->rxScheduler;
        unschedByteAllocator = sxController->unschedByteAllocator;
        homaConfig = rxScheduler->homaConfig;
        schedSenders = rxScheduler->schedSenders;
        socket = (MockUdpSocket*)(&(transport->socket));
    }

    ~HomaTransportTest()
    {
        delete transport;
    }

    std::pair<AppMessage*, HomaTransport::OutboundMessage*>
    newMesg(uint32_t mesgSize, simtime_t creationTime, uint64_t mesgId,
            inet::L3Address srcAddr, inet::L3Address destAddr) {
        AppMessage* msg = new AppMessage();
        msg->setMsgCreationTime(creationTime);
        msg->setByteLength(mesgSize);
        msg->setDestAddr(destAddr);
        msg->setSrcAddr(srcAddr);

        std::vector<uint16_t> reqUnschedDataVec =
            unschedByteAllocator->getReqUnschedDataPkts(
            srcAddr.toIPv4().getInt(), msg->getByteLength());
        auto outMesg = new HomaTransport::OutboundMessage(msg, sxController,
            mesgId, reqUnschedDataVec);
        outMesg->prepareRequestAndUnsched();

        return std::make_pair(msg, outMesg);
    }

    simtime_t
    pktTimeLen(HomaPkt* pkt) {
        uint32_t pktLenOnWire = HomaPkt::getBytesOnWire(pkt->getDataBytes(),
            (PktType)pkt->getPktType());
        simtime_t pktDuration = SimTime(1e-9 * (pktLenOnWire * 8.0 /
            rxScheduler->homaConfig->nicLinkSpeed));
        return pktDuration;
    }

  private:
    DISALLOW_COPY_AND_ASSIGN(HomaTransportTest);
};


TEST_F (HomaTransportTest, basic) {
    ((cSimpleModule*)netModule)->wait(SimTime(1e-15));
    simtime_t timeNow = simTime();

    // Conctruct a mesg from sender 2 destined to test transport
    inet::L3Address srcAddr("10.0.0.2");
    uint64_t mesgId = 0;
    auto mesgPair = newMesg(15000, timeNow, mesgId,
        srcAddr, localAddr);
    auto appMsg1 = mesgPair.first;
    auto outMsg1 = mesgPair.second;

    // receive first packet of the message 1 and start processing
    HomaPkt* pkt = NULL;
    outMsg1->getTransmitReadyPkt(&pkt);
    HomaPkt cpyPkt1 = *pkt;
    auto pktDuration = pktTimeLen(pkt);
    ASSERT_TRUE(pkt->getFirstByte() == 0 &&
        pkt->getPktType() == PktType::REQUEST);
    transport->handleRecvdPkt(pkt);
    HomaTransport::ReceiveScheduler::SenderState* ss1 =
        rxScheduler->ipSendersMap[srcAddr.toIPv4().getInt()];
    ASSERT_EQ(ss1->lastGrantPrio, homaConfig->allPrio-1);

    // check inbound message instance created for this message
    HomaTransport::InboundMessage* inbMesg1 =
        rxScheduler->lookupInboundMesg(&cpyPkt1);
    ASSERT_EQ(schedSenders->senders.size(),
        homaConfig->adaptiveSchedPrioLevels);
    ASSERT_TRUE(inbMesg1->msgIdAtSender == mesgId &&
        inbMesg1->lastGrantTime == timeNow);
    ASSERT_EQ(inbMesg1->bytesToReceive,
        appMsg1->getByteLength() - cpyPkt1.getDataBytes());

    // retrieve sent grant and add scheduled pkt corresponding to that
    // grant to the txPkts queue.
    HomaPkt* grantPkt = socket->getGrantPkt(srcAddr, mesgId);
    ASSERT_TRUE(grantPkt);
    outMsg1->prepareSchedPkt(grantPkt->getGrantFields().grantBytes,
        grantPkt->getGrantFields().schedPrio);
    ASSERT_EQ(outMsg1->txSchedPkts.size(), 1);
    delete grantPkt;

    // wait for a packet time and receive a new mesg 2 from a different sender
    ((cSimpleModule*)netModule)->wait(pktDuration);
    srcAddr = inet::L3Address("10.0.0.3");
    mesgId = 1;
    timeNow = simTime();
    mesgPair = newMesg(35000, timeNow, mesgId, srcAddr, localAddr);
    auto appMsg2 = mesgPair.first;
    auto outMsg2 = mesgPair.second;
    outMsg2->getTransmitReadyPkt(&pkt);
    HomaPkt cpyPkt2 = *pkt;
    transport->handleRecvdPkt(pkt);

    // check grant is sent for this message as well
    HomaTransport::ReceiveScheduler::SenderState* ss2 =
        rxScheduler->ipSendersMap[srcAddr.toIPv4().getInt()];
    HomaTransport::InboundMessage* inbMesg2 =
        rxScheduler->lookupInboundMesg(&cpyPkt2);
    ASSERT_EQ(ss2->lastGrantPrio, homaConfig->allPrio-1);
    ASSERT_TRUE(inbMesg2->msgIdAtSender == mesgId &&
        inbMesg2->lastGrantTime == timeNow &&
        inbMesg2->bytesGrantedInFlight > 0);

    // Wait another packet time and shorter mesg 3 from another sender arrives
    ((cSimpleModule*)netModule)->wait(pktDuration);
    srcAddr = inet::L3Address("10.0.0.4");
    mesgId = 2;
    timeNow = simTime();
    mesgPair = newMesg(25000, timeNow, mesgId, srcAddr, localAddr);
    auto appMsg3 = mesgPair.first;
    auto outMsg3 = mesgPair.second;
    outMsg3->getTransmitReadyPkt(&pkt);
    HomaPkt cpyPkt3 = *pkt;
    transport->handleRecvdPkt(pkt);

    // check grant is sent for this message as well with the right priority
    HomaTransport::ReceiveScheduler::SenderState* ss3 =
        rxScheduler->ipSendersMap[srcAddr.toIPv4().getInt()];
    HomaTransport::InboundMessage* inbMesg3 =
        rxScheduler->lookupInboundMesg(&cpyPkt3);
    ASSERT_EQ(ss3->lastGrantPrio, homaConfig->allPrio-2);
    ASSERT_TRUE(inbMesg3->msgIdAtSender == mesgId &&
        inbMesg3->lastGrantTime == timeNow &&
        inbMesg3->bytesGrantedInFlight > 0);

    // Wait another packet time and a larger mesg 4 from another sender arrives
    ((cSimpleModule*)netModule)->wait(pktDuration);
    srcAddr = inet::L3Address("10.0.0.5");
    mesgId = 3;
    timeNow = simTime();
    mesgPair = newMesg(45000, timeNow, mesgId, srcAddr, localAddr);
    auto appMsg4 = mesgPair.first;
    auto outMsg4 = mesgPair.second;
    outMsg4->getTransmitReadyPkt(&pkt);
    HomaPkt cpyPkt4 = *pkt;
    transport->handleRecvdPkt(pkt);

    // check no grant is sent for this last message since it's beyond the number
    // of schedPrioLevels
    HomaTransport::InboundMessage* inbMesg4 =
        rxScheduler->lookupInboundMesg(&cpyPkt4);
    ASSERT_TRUE(inbMesg4->msgIdAtSender == mesgId &&
        inbMesg4->bytesGrantedInFlight == 0);

    // Wait another packet time and a larger mesg 5 from another sender arrives
    ((cSimpleModule*)netModule)->wait(pktDuration);
    srcAddr = inet::L3Address("10.0.0.6");
    mesgId = 4;
    timeNow = simTime();
    mesgPair = newMesg(55000, timeNow, mesgId, srcAddr, localAddr);
    auto appMsg5 = mesgPair.first;
    auto outMsg5 = mesgPair.second;
    outMsg5->getTransmitReadyPkt(&pkt);
    HomaPkt cpyPkt5 = *pkt;
    transport->handleRecvdPkt(pkt);

    // check no grant is sent for this last message since it's beyond the number
    // of schedPrioLevels
    HomaTransport::InboundMessage* inbMesg5 =
        rxScheduler->lookupInboundMesg(&cpyPkt5);
    ASSERT_TRUE(inbMesg5->msgIdAtSender == mesgId &&
        inbMesg5->bytesGrantedInFlight == 0);

    uint16_t txPktsNum = 1;
    uint32_t bytesGrantedInFlightOld;
    while (inbMesg1->bytesToReceive > 0) {
        // From this point on, for every packet received from each top
        // numToGrant sender, the receiver sends a new grant at the right
        // priority.
        bytesGrantedInFlightOld = inbMesg1->bytesGrantedInFlight;
        outMsg1->getTransmitReadyPkt(&pkt);
        pktDuration = pktTimeLen(pkt);
        cpyPkt1 = *pkt;
        ((cSimpleModule*)netModule)->wait(pktDuration);
        timeNow = simTime();
        if (inbMesg1->bytesToGrant > 0) {
            transport->handleRecvdPkt(pkt);
            ASSERT_EQ(ss1->lastGrantPrio, homaConfig->allPrio-3);
            ASSERT_TRUE(inbMesg1->lastGrantTime == timeNow &&
                inbMesg1->bytesGrantedInFlight - bytesGrantedInFlightOld > 0);

            // retrieve sent grant and add scheduled pkt corresponding to that
            // grant to the txPkts queue.
            grantPkt = socket->getGrantPkt(inbMesg1->srcAddr,
                inbMesg1->msgIdAtSender);
            ASSERT_TRUE(grantPkt);
            outMsg1->prepareSchedPkt(grantPkt->getGrantFields().grantBytes,
                grantPkt->getGrantFields().schedPrio);

            if (cpyPkt1.getPktType() == PktType::UNSCHED_DATA) {
                // As long as there are unsched pkts to be transmitted, schedPkt
                // queue length keeps increasing since unscheduled packets are
                // prioritiezed for transmission over sched packets. We test
                // this here
                txPktsNum++;
            }
            ASSERT_EQ(outMsg1->txSchedPkts.size(), txPktsNum);
            delete grantPkt;
        } else {
            transport->handleRecvdPkt(pkt);
            grantPkt = socket->getGrantPkt(inbMesg1->srcAddr,
                inbMesg1->msgIdAtSender);
            ASSERT_FALSE(grantPkt);
        }
    }

    // Create a new mesg from a different sender and check no grant is sent for
    // this message
    srcAddr = inet::L3Address("10.0.0.7");
    mesgId = 5;
    timeNow = simTime();
    mesgPair = newMesg(70000, timeNow, mesgId, srcAddr, localAddr);
    auto appMsg6 = mesgPair.first;
    auto outMsg6 = mesgPair.second;
    outMsg6->getTransmitReadyPkt(&pkt);
    HomaPkt cpyPkt6 = *pkt;
    pktDuration = pktTimeLen(pkt);
    ((cSimpleModule*)netModule)->wait(pktDuration);
    transport->handleRecvdPkt(pkt);

    // check no grant is sent for this last message since it's beyond the number
    // of schedPrioLevels
    HomaTransport::InboundMessage* inbMesg6 =
        rxScheduler->lookupInboundMesg(&cpyPkt6);
    ASSERT_TRUE(inbMesg6->msgIdAtSender == mesgId &&
        inbMesg6->bytesGrantedInFlight == 0);

    // get a packet from next highest prio message and check grant at second
    // highest sched prio is sent for that message
    outMsg3->getTransmitReadyPkt(&pkt);
    cpyPkt3 = *pkt;
    pktDuration = pktTimeLen(pkt);
    ((cSimpleModule*)netModule)->wait(pktDuration);
    timeNow = simTime();
    bytesGrantedInFlightOld = inbMesg3->bytesGrantedInFlight;
    transport->handleRecvdPkt(pkt);
    ASSERT_EQ(ss3->lastGrantPrio, homaConfig->allPrio-3);
    ASSERT_TRUE(inbMesg3->lastGrantTime == timeNow &&
        inbMesg3->bytesGrantedInFlight - bytesGrantedInFlightOld > 0);

    // get a packet from second highest prio message and check grant at second
    // second highest sched prio is sent for that message
    outMsg2->getTransmitReadyPkt(&pkt);
    cpyPkt2 = *pkt;
    pktDuration = pktTimeLen(pkt);
    ((cSimpleModule*)netModule)->wait(pktDuration);
    timeNow = simTime();
    bytesGrantedInFlightOld = inbMesg2->bytesGrantedInFlight;
    transport->handleRecvdPkt(pkt);
    ASSERT_EQ(ss2->lastGrantPrio, homaConfig->allPrio-2);
    ASSERT_TRUE(inbMesg2->lastGrantTime == timeNow &&
        inbMesg2->bytesGrantedInFlight - bytesGrantedInFlightOld > 0);

    // wait for 1.5 pkt time, and check that a non granting sender is introduced
    // into set of granting senders.
    ((cSimpleModule*)netModule)->wait(rxScheduler->bwCheckInterval);


    delete appMsg1;
    delete appMsg2;
    delete appMsg3;
    delete appMsg4;
    delete appMsg5;
    delete appMsg6;

    delete outMsg1;
    delete outMsg2;
    delete outMsg3;
    delete outMsg4;
    delete outMsg5;
    delete outMsg6;
}
