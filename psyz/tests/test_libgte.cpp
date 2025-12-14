#include <gtest/gtest.h>
extern "C" {
#include "common.h"
#include "kernel.h"
#include "libgte.h"
}

class gte_Test : public testing::Test {
  protected:
    gte_Test() {}
    ~gte_Test() override = default;
    void SetUp() override {
    }
    void TearDown() override {
    }
};

TEST_F(gte_Test, rsin) {
    EXPECT_EQ(rsin(0x0000), 0x0000);
    EXPECT_EQ(rsin(0x0001), 0x0006);
    EXPECT_EQ(rsin(0x0002), 0x000D);
    EXPECT_EQ(rsin(0x0010), 0x0065);
    EXPECT_EQ(rsin(0x0100), 0x061F);
    EXPECT_EQ(rsin(0x0200), 0x0B50);
    EXPECT_EQ(rsin(0x0400), 0x1000);
    EXPECT_EQ(rsin(0x0800), 0x0000);
    EXPECT_EQ(rsin(0x1000), 0x0000);
}
