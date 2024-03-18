
#include <limits>

#include <gtest/gtest.h>

#include "controller/commands/tokens/ServoPosition.h"
#include "device/ServoSpecifier.h"


TEST(ServoPosition, Create) {

    auto id = ServoSpecifier(creatures::config::UARTDevice::A, 0);
    auto position = std::make_shared<creatures::ServoPosition>(id, std::numeric_limits<u32>::max());

    EXPECT_EQ(id, position->getServoId());
    EXPECT_EQ(std::numeric_limits<u32>::max(), position->getRequestedTicks());
}

TEST(ServoPosition, ToString) {

    auto id = ServoSpecifier(creatures::config::UARTDevice::A, 6);
    auto position = std::make_shared<creatures::ServoPosition>(id, 10923);

    EXPECT_EQ("6 10923", position->toString());
}