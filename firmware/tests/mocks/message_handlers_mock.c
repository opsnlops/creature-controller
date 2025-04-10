/**
 * @file message_handlers_mock.c
 * @brief Mock implementations of message handler functions
 */

#include "messaging/messaging.h"
#include <stdbool.h>

// Mock implementations of the message handler functions
bool handlePingMessage(const GenericMessage *msg) {
    return true;
}

bool handlePositionMessage(const GenericMessage *msg) {
    return true;
}

bool handleConfigMessage(const GenericMessage *msg) {
    return true;
}