
#include <fmt/format.h>

#include "controller-config.h"

#include "StatsMessage.h"

namespace creatures {

    StatsMessage::StatsMessage() {
        freeHeap = 0;
        charactersReceived = 0;
        messagesReceived = 0;
        messagesSent = 0;
    }

    std::string StatsMessage::toString() const {
        return fmt::format("[firmware stats] Free heap: {}, Characters received: {}, Messages received: {}, Messages sent: {}",
            freeHeap, charactersReceived, messagesReceived, messagesSent);
    }

} // creatures