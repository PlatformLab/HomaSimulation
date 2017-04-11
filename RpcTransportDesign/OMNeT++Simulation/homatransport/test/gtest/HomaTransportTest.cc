#include <gtest/gtest.h>
#include "TestUtil.h"
#include "common/Minimal.h"
#include "transport/HomaTransport.h"
#include "TestMain.h"


class HomaTransportTest : public ::testing::Test {
  public:
    HomaTransport* transport;
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
        //std::cout << "transport config addr " << static_cast<void*>(transport->homaConfig) << std::endl;
        //std::cout << "transport config addr " << static_cast<void*>(transport->rxScheduler.homaConfig) << std::endl;
        //std::cout << "transport config addr " << static_cast<void*>(transport->sxController.homaConfig) << std::endl;
        //std::cout << "Homa instance created." << std::endl;
        //std::cout << transport->rxScheduler.homaConfig->destPort << std::endl;
        //std::cout << transport->rxScheduler.homaConfig->destPort << "\t" <<
        //    transport->rxScheduler.homaConfig->defaultUnschedBytes << std::endl;

    }

    ~HomaTransportTest()
    {
        delete transport;
    }
    
  private:
    DISALLOW_COPY_AND_ASSIGN(HomaTransportTest);
};


TEST_F(HomaTransportTest, basic) {
    AppMessage* msg = new AppMessage();
    inet::L3Address destAddrs("10.0.0.1");
    inet::L3Address srcAddrs("10.0.0.2");
    uint32_t mesgSize = 20000;
    msg->setMsgCreationTime(simTime()); 
    msg->setByteLength(mesgSize);
    msg->setDestAddr(destAddrs);
    msg->setSrcAddr(srcAddrs);
    std::vector<uint16_t> reqUnschedDataVec =
        transport->sxController.unschedByteAllocator->getReqUnschedDataPkts(
        destAddrs.toIPv4().getInt(), mesgSize);
    HomaTransport::OutboundMessage outMesg(msg, &transport->sxController,
        0, reqUnschedDataVec);
    outMesg.prepareRequestAndUnsched();
    /********
    std::cout << outMesg.msgId << "\t" <<  outMesg.msgSize << "\t" <<
        outMesg.bytesToSched << "\t" <<  outMesg.nextByteToSched << "\t" <<
        outMesg.bytesLeft << "\t" << outMesg.unschedBytesLeft << "\t" <<
        std::endl;
    std::cout << transport->homaConfig->defaultUnschedBytes << "\t" <<
        transport->homaConfig->destPort << std::endl;
    *******/
    HomaPkt* pkt = NULL;
    outMesg.getTransmitReadyPkt(&pkt);
    ASSERT_EQ(pkt->getFirstByte(), 0);
    //transport->rxScheduler.processReceivedPkt(pkt);
    //((cSimpleModule*)netModule)->wait(SimTime(1e-6));
    delete msg;
}
