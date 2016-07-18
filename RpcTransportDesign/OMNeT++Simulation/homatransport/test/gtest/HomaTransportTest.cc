#include <gtest/gtest.h>
#include "TestUtil.h"
#include "common/Minimal.h"
#include "transport/HomaTransport.h"

class HomaTransportTest : public ::testing::Test {
  public:
    HomaTransport* transport;
    HomaTransportTest()
    {
        cModuleType* transportType =
            cModuleType::get("homatransport.transport.HomaTransport");
        (cModule*) transport = transportType->create("transport", this);
        transport->finalizeParameters();
        transport->buildInside();
        transport->scheduleStart(simTime());
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
    msg->setMsgCreationTime(simTime()); 
    msg->setByteLength(10000);
    delete msg;
}
