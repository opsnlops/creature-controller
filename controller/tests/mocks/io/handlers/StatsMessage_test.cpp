
#include <gtest/gtest.h>
#include "io/handlers/StatsMessage.h"

TEST(StatsMessage, Create) {

    EXPECT_NO_THROW({
                        auto statsMessage = creatures::StatsMessage();
                        statsMessage.charactersReceived = 123;
                        statsMessage.messagesReceived = 456;
                        statsMessage.freeHeap = 1111111;
                        statsMessage.messagesSent = 789;
                        statsMessage.parseSuccesses = 10;
                        statsMessage.parseFailures = 20;
                        statsMessage.checksumFailures = 30;
                        statsMessage.positionMessagesProcessed = 40;
                    });
}

TEST(StatsMessage, ToString) {

    auto statsMessage = creatures::StatsMessage();
    statsMessage.charactersReceived = 123;
    statsMessage.messagesReceived = 456;
    statsMessage.freeHeap = 1111111;
    statsMessage.messagesSent = 789;
    statsMessage.parseSuccesses = 10;
    statsMessage.parseFailures = 20;
    statsMessage.checksumFailures = 30;
    statsMessage.positionMessagesProcessed = 40;

    EXPECT_EQ("[firmware stats] heap: 1111111, chars: 123, mesg_rec: 456, mesg_sent: 789, parse_suc: 10, parse_fail: 20, cksum_fail: 30, pos_proc: 40",
              statsMessage.toString());
}
