
#include <fmt/format.h>

#include "controller-config.h"

#include "StatsMessage.h"

namespace creatures {

    StatsMessage::StatsMessage() {
        freeHeap = 0UL;
        charactersReceived = 0UL;
        messagesReceived = 0UL;
        messagesSent = 0UL;
        parseSuccesses = 0UL;
        parseFailures = 0UL;
        checksumFailures = 0UL;
        positionMessagesProcessed = 0UL;
        pwmWraps = 0UL;
    }

    std::string StatsMessage::toString() const {
        return fmt::format("[firmware stats] heap: {}, chars: {}, mesg_rec: {}, mesg_sent: {}, "
                           "parse_suc: {}, parse_fail: {}, cksum_fail: {}, pos_proc: {}, pwm_wraps: {}",
            freeHeap, charactersReceived, messagesReceived, messagesSent, parseSuccesses,
            parseFailures, checksumFailures, positionMessagesProcessed, pwmWraps);
    }

} // creatures