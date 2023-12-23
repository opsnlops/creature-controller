
#pragma once

#include <string>

#include "controller-config.h"

namespace creatures {

    class StatsMessage {

    public:
        StatsMessage();
        ~StatsMessage() = default;

        std::string toString() const;

        u64 freeHeap;
        u64 charactersReceived;
        u64 messagesReceived;
        u64 messagesSent;
        u64 parseSuccesses;
        u64 parseFailures;
        u64 checksumFailures;
        u64 positionMessagesProcessed;

    };

} // creatures


