
#pragma once

#include <string>

#include "controller-config.h"

namespace creatures {

    class StatsMessage {

    public:
        StatsMessage();
        ~StatsMessage() = default;

        std::string toString() const;

        u32 freeHeap;
        u32 charactersReceived;
        u32 messagesReceived;
        u32 messagesSent;

    };

} // creatures


