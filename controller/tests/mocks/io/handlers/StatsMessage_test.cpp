
#include <gtest/gtest.h>
#include "io/handlers/StatsMessage.h"

TEST(StatsMessage, Create) {

    EXPECT_NO_THROW({
                        auto statsMessage = creatures::StatsMessage();
                        statsMessage.charactersReceived = 123;
                        statsMessage.messagesReceived = 456;
                        statsMessage.freeHeap = 1111111;
                        statsMessage.messagesSent = 789;
                    });
}

TEST(StatsMessage, ToString) {

    auto statsMessage = creatures::StatsMessage();
    statsMessage.charactersReceived = 123;
    statsMessage.messagesReceived = 456;
    statsMessage.freeHeap = 1111111;
    statsMessage.messagesSent = 789;

    EXPECT_EQ("[firmware stats] Free heap: 1111111, Characters received: 123, Messages received: 456, Messages sent: 789",
              statsMessage.toString());
}
