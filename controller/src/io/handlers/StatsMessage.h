
#pragma once

#include <string>

#include "controller-config.h"

// Stats messages from the firmware
#define STATS_MESSAGE "STATS"

// Memory
#define STATS_HEAP_FREE "HEAP_FREE"

// USB Pipeline
#define STATS_USB_CHARACTERS_RECEIVED "USB_CRECV"
#define STATS_USB_MESSAGES_RECEIVED "USB_MRECV"
#define STATS_USB_MESSAGES_SENT "USB_SENT"

// UART Pipeline
#define STATS_UART_CHARACTERS_RECEIVED "UART_CRECV"
#define STATS_UART_MESSAGES_RECEIVED "UART_MRECV"
#define STATS_UART_MESSAGES_SENT "UART_SENT"

// Message Processor
#define STATS_MP_MESSAGES_RECEIVED "MP_RECV"
#define STATS_MP_MESSAGES_SENT "MP_SENT"

// Parsing
#define STATS_SUCCESSFUL_PARSE "S_PARSE"
#define STATS_FAILED_PARSE "F_PARSE"
#define STATS_CHECKSUM_FAILED "CHKFAIL"

// Movement
#define STATS_POSITIONS_PROCESSED "POS_PROC"

// PWM
#define STATS_PWM_WRAPS "PWM_WRAPS"

// Board sensors
#define STATS_BOARD_TEMPERATURE "TEMP"

// Dynamixel bus metrics
#define STATS_DXL_TX "DXL_TX"
#define STATS_DXL_RX "DXL_RX"
#define STATS_DXL_ERR "DXL_ERR"
#define STATS_DXL_CRC "DXL_CRC"
#define STATS_DXL_TO "DXL_TO"

namespace creatures {

class StatsMessage {

  public:
    StatsMessage();
    ~StatsMessage() = default;

    std::string toString() const;

    u64 freeHeap;
    u64 uSBCharactersReceived;
    u64 uSBMessagesReceived;
    u64 uSBMessagesSent;

    u64 uARTCharactersReceived;
    u64 uARTMessagesReceived;
    u64 uARTMessagesSent;

    u64 mPMessagesReceived;
    u64 mPMessagesSent;

    u64 parseSuccesses;
    u64 parseFailures;
    u64 checksumFailures;
    u64 positionMessagesProcessed;
    u64 pwmWraps;

    double boardTemperature;

    // Dynamixel bus metrics
    u64 dxlTxPackets;
    u64 dxlRxPackets;
    u64 dxlErrors;
    u64 dxlCrcErrors;
    u64 dxlTimeouts;
};

} // namespace creatures
