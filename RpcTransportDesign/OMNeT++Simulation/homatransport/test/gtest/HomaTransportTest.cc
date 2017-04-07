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
        cModuleType* transportType =
            cModuleType::get("homatransport.transport.HomaTransport");
        
        /***********************
        transport = (HomaTransport*)transportType->create("transport", NULL);
        transport->finalizeParameters();
        transport->buildInside();
        transport->scheduleStart(simTime());
        transport->callInitialize();
        ************************/
        /*************************
        std::cout << "assigning netModule value: " <<
            static_cast<void*>(netModule) << std::endl;
        **************************/

        transport = (HomaTransport*)transportType->createScheduleInit(
            "transport", netModule);
    }

    ~HomaTransportTest()
    {
        delete transport;
    }
    
  private:
    DISALLOW_COPY_AND_ASSIGN(HomaTransportTest);
};


TEST_F(HomaTransportTest, basic) {
    ((cSimpleModule*)netModule)->wait(SimTime(1e-6));
    std::cout << "The control returned to test fixture" << std::endl;
    AppMessage* msg = new AppMessage();
    msg->setMsgCreationTime(simTime()); 
    msg->setByteLength(10000);
    delete msg;
}
