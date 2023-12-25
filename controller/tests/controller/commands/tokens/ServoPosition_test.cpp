
#include <limits>

#include <gtest/gtest.h>


#include "controller/commands/tokens/ServoPosition.h"

TEST(ServoPosition, Create) {

    auto position = std::make_shared<creatures::ServoPosition>("A0", std::numeric_limits<u32>::max());

    EXPECT_EQ("A0", position->getOutputPosition());
    EXPECT_EQ(std::numeric_limits<u32>::max(), position->getRequestedTicks());
}

TEST(ServoPosition, ToString) {

    auto position = std::make_shared<creatures::ServoPosition>("A0", 10923);

    EXPECT_EQ("A0 10923", position->toString());
}