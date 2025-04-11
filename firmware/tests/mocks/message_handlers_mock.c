/**
 * @file message_handlers_mock.c
 * @brief Mock implementations of message handler functions
 */

#include <stdbool.h>
#include "messaging/messaging.h"

// Mock implementations of the message handler functions
bool handlePingMessage(const void *msg) {
    // Cast to the right type inside the implementation
    // const GenericMessage *real_msg = (const GenericMessage *)msg;

    // Simple mock that just returns success
    return true;
}

bool handlePositionMessage(const void *msg) {
    // Cast to the right type inside the implementation
    // const GenericMessage *real_msg = (const GenericMessage *)msg;

    // Simple mock that just returns success
    return true;
}

bool handleConfigMessage(const void *msg) {
    // Cast to the right type inside the implementation
    // const GenericMessage *real_msg = (const GenericMessage *)msg;

    // Simple mock that just returns success
    return true;
}