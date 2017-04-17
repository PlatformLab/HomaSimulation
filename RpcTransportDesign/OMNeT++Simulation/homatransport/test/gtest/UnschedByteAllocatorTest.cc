#include <gtest/gtest.h>
#include "TestUtil.h"
#include "common/Minimal.h"
#include "transport/UnschedByteAllocator.h"

class UnschedByteAllocatorTest : public ::testing::Test {
  public:
    UnschedByteAllocator unschedBytesAllocator;
    UnschedByteAllocatorTest()
        : unschedBytesAllocator(NULL)
    {}

  private:
    DISALLOW_COPY_AND_ASSIGN(UnschedByteAllocatorTest);
};

TEST_F (UnschedByteAllocatorTest, basic) {
    EXPECT_EQ(unschedBytesAllocator.rxAddrReqbyteMap.size(), 0);
}

