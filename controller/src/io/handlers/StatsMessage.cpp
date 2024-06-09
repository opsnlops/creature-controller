
#include <fmt/format.h>

#include "controller-config.h"

#include "StatsMessage.h"

namespace creatures {

    StatsMessage::StatsMessage() {
        freeHeap = 0UL;
        uSBCharactersReceived = 0UL;
        uSBMessagesReceived = 0UL;
        uSBMessagesSent = 0UL;
        uARTCharactersReceived = 0UL;
        uARTMessagesReceived = 0UL;
        uARTMessagesSent = 0UL;
        mPMessagesReceived = 0UL;
        mPMessagesSent = 0UL;

        parseSuccesses = 0UL;
        parseFailures = 0UL;
        checksumFailures = 0UL;
        positionMessagesProcessed = 0UL;
        pwmWraps = 0UL;
    }

    std::string StatsMessage::toString() const {
        return fmt::format("heap: {}, usb_chars: {}, usb_mesg_rec: {}, usb_mesg_sent: {}, "
                           "uart_chars: {}, uart_mesg_rec: {}, uart_mesg_sent: {}, "
                           "mp_recv: {}, mp_sent: {}, "
                           "parse_suc: {}, parse_fail: {}, cksum_fail: {}, pos_proc: {}, pwm_wraps: {}",
            freeHeap,
            uSBCharactersReceived, uSBMessagesReceived, uSBMessagesSent,
            uARTCharactersReceived, uARTMessagesReceived, uARTMessagesSent,
            mPMessagesReceived, mPMessagesSent,
            parseSuccesses, parseFailures, checksumFailures,
            positionMessagesProcessed,
            pwmWraps);
    }

} // creatures