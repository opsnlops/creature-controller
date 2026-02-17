
#include "io/handlers/StatsMessage.h"
#include <gtest/gtest.h>

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
        statsMessage.pwmWraps = 3000UL;
        statsMessage.boardTemperature = 75.2;
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
    statsMessage.boardTemperature = 75.2;

    statsMessage.dxlTxPackets = 500;
    statsMessage.dxlRxPackets = 400;
    statsMessage.dxlErrors = 1;
    statsMessage.dxlCrcErrors = 2;
    statsMessage.dxlTimeouts = 3;

    EXPECT_EQ(
        "heap: 36920, usb_chars: 123, usb_mesg_rec: 456, usb_mesg_sent: 789, uart_chars: 321, uart_mesg_rec: 654, "
        "uart_mesg_sent: 987, mp_recv: 1111, mp_sent: 222, parse_suc: 10, parse_fail: 20, cksum_fail: 30, pos_proc: "
        "2789, pwm_wraps: 3000, temp: 75.20, dxl_tx: 500, dxl_rx: 400, dxl_err: 1, dxl_crc: 2, dxl_to: 3",
        statsMessage.toString());
}
