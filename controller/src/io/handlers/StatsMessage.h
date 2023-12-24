
#pragma once

#include <string>

#include "controller-config.h"


// Stats messages from the firmware
#define STATS_MESSAGE                       "STATS"
#define STATS_HEAP_FREE                     "HEAP_FREE"
#define STATS_CHARACTERS_RECEIVED           "C_RECV"
#define STATS_MESSAGES_RECEIVED             "M_RECV"
#define STATS_MESSAGES_SENT                 "SENT"
#define STATS_SUCCESSFUL_PARSE              "S_PARSE"
#define STATS_FAILED_PARSE                  "F_PARSE"
#define STATS_CHECKSUM_FAILED               "CHKFAIL"
#define STATS_POSITIONS_PROCESSED           "POS_PROC"
#define STATS_PWM_WRAPS                     "PWM_WRAPS"


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
        u64 pwmWraps;

    };

} // creatures


