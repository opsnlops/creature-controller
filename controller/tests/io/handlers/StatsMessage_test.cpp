
#include <gtest/gtest.h>
#include "io/handlers/StatsMessage.h"

TEST(StatsMessage, Create) {

    EXPECT_NO_THROW({
                        auto statsMessage = creatures::StatsMessage();
                        statsMessage.uSBCharactersReceived = 123;
                        statsMessage.uSBMessagesReceived = 456;
                        statsMessage.uSBMessagesSent = 789;
                        statsMessage.uARTCharactersReceived = 321;
                        statsMessage.uARTMessagesReceived = 654;
                        statsMessage.uARTMessagesSent = 987;
                        statsMessage.mPMessagesReceived = 1111;
                        statsMessage.mPMessagesSent = 222;
                        statsMessage.freeHeap = 36920;
                        statsMessage.parseSuccesses = 10;
                        statsMessage.parseFailures = 20;
                        statsMessage.checksumFailures = 30;
                        statsMessage.positionMessagesProcessed = 2789;
                        statsMessage.pwmWraps = 3000UL;;
                    });
}

TEST(StatsMessage, ToString) {

    auto statsMessage = creatures::StatsMessage();
    statsMessage.uSBCharactersReceived = 123;
    statsMessage.uSBMessagesReceived = 456;
    statsMessage.uSBMessagesSent = 789;
    statsMessage.uARTCharactersReceived = 321;
    statsMessage.uARTMessagesReceived = 654;
    statsMessage.uARTMessagesSent = 987;
    statsMessage.mPMessagesReceived = 1111;
    statsMessage.mPMessagesSent = 222;
    statsMessage.freeHeap = 36920;
    statsMessage.parseSuccesses = 10;
    statsMessage.parseFailures = 20;
    statsMessage.checksumFailures = 30;
    statsMessage.positionMessagesProcessed = 2789;
    statsMessage.pwmWraps = 3000UL;

    EXPECT_EQ("[firmware] heap: 36920, usb_chars: 123, usb_mesg_rec: 456, usb_mesg_sent: 789, uart_chars: 321, uart_mesg_rec: 654, uart_mesg_sent: 987, mp_recv: 1111, mp_sent: 222, parse_suc: 10, parse_fail: 20, cksum_fail: 30, pos_proc: 2789, pwm_wraps: 3000",
              statsMessage.toString());
}
