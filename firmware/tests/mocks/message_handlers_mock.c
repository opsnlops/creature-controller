/**
 * @file message_handlers_mock.c
 * @brief Mock implementations of message handler functions
 */

#include "messaging/messaging.h"
#include <stdbool.h>
#include <stdio.h>

// Mock implementations of the message handler functions
bool handlePingMessage(const GenericMessage *msg) {
    if (msg == NULL) {
        printf("WARNING: handlePingMessage called with NULL message\n");
        return false;
    }

    // Simple validation to avoid crashes
    if (msg->messageType[0] == '\0') {
        printf("INFO: handlePingMessage called with empty message type\n");
    }

    return true;
}

bool handlePositionMessage(const GenericMessage *msg) {
    if (msg == NULL) {
        printf("WARNING: handlePositionMessage called with NULL message\n");
        return false;
    }

    // Simple validation to avoid crashes
    if (msg->messageType[0] == '\0') {
        printf("INFO: handlePositionMessage called with empty message type\n");
    }

    return true;
}

bool handleConfigMessage(const GenericMessage *msg) {
    if (msg == NULL) {
        printf("WARNING: handleConfigMessage called with NULL message\n");
        return false;
    }

    // Simple validation to avoid crashes
    if (msg->messageType[0] == '\0') {
        printf("INFO: handleConfigMessage called with empty message type\n");
    }

    return true;
}

// REMOVED duplicate calculateChecksum function