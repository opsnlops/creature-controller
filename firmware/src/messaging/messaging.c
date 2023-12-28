
#include <stdbool.h>
#include <stddef.h>
#include <string.h>


#include "logging/logging.h"
#include "messaging/messaging.h"
#include "util/string_utils.h"

#include "controller-config.h"

// Various handlers
#include "messaging/processors/ping_message.h"
#include "messaging/processors/position_message.h"

// Keep stats
volatile u64 successful_messages_parsed = 0UL;
volatile u64 failed_messages_parsed = 0UL;
volatile u64 checksum_errors = 0UL;

const MessageTypeHandler messageHandlers[] = {
        {"PING", handlePingMessage},
        {"POS",  handlePositionMessage},
};


u16 calculateChecksum(const char *message) {
    u16 checksum = 0;

    if (message != NULL) {
        while (*message != '\0') {
            checksum += (u8) (*message);
            message++;
        }
    }
    verbose("checksum: %u", checksum);

    return checksum;
}


bool parseMessage(const char *rawMessage, GenericMessage *outMessage) {

    // Temporary buffer to hold parts of the message
    char buffer[USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH];
    memset(buffer, '\0', USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);

    strncpy(buffer, rawMessage, USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);
    buffer[USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH - 1] = '\0';

    // Tokenize the message
    char *token = strtok(buffer, "\t");
    int tokenIndex = 0;

    while (token != NULL && tokenIndex < MAX_TOKENS - 1) {
        if (tokenIndex == 0) {
            // First token is the message type
            strncpy(outMessage->messageType, token, MESSAGE_ACTION_MAX_SIZE);
            outMessage->messageType[MESSAGE_ACTION_MAX_SIZE - 1] = '\0';
        } else {
            // Subsequent tokens are the message content
            strncpy(outMessage->tokens[tokenIndex - 1], token, MAX_TOKEN_LENGTH);
            outMessage->tokens[tokenIndex - 1][MAX_TOKEN_LENGTH - 1] = '\0';
        }

        token = strtok(NULL, "\t");
        tokenIndex++;
    }

    if (tokenIndex < 2) {
        // Not enough tokens (at least messageType and checksum are needed)
        warning("not enough tokens in message: %s", rawMessage);
        return false;
    }

    outMessage->tokenCount = tokenIndex - 2; // Exclude checksum token in count

    // Extract the expected checksum from the last token
    char *checksumToken = outMessage->tokens[outMessage->tokenCount];
    char *spacePos = strchr(checksumToken, ' ');
    if (spacePos != NULL && *(spacePos + 1) != '\0') {
        outMessage->expectedChecksum = stringToU16(spacePos + 1);  // Convert the part after the space
    } else {
        // Checksum token format error
        return false;
    }


    // Find the position of the last tab character, which precedes the checksum token
    const char *lastTabPos = strrchr(rawMessage, '\t');
    if (lastTabPos != NULL) {
        // Calculate the length of the message part that should be included in the checksum calculation
        size_t checksumLength = lastTabPos - rawMessage;

        // Create a temporary buffer to hold this part of the message
        char checksumPart[checksumLength + 1]; // +1 for null terminator
        strncpy(checksumPart, rawMessage, checksumLength);

        checksumPart[checksumLength] = '\0'; // Null terminate the string

        // Calculate the checksum
        outMessage->calculatedChecksum = calculateChecksum(checksumPart);
    } else {
        // If no tab character was found, the message format is invalid
        return false;
    }

    return true;
}

void processMessage(const char *rawMessage) {

    verbose("processing message: %s", rawMessage);

    // Create a message object and null it out for safety
    GenericMessage msg;
    memset(&msg, '\0', sizeof(GenericMessage));

    // Parse the message
    if (!parseMessage(rawMessage, &msg)) {
        error("unable to parse incoming message: %s", rawMessage);
        failed_messages_parsed = failed_messages_parsed + 1;
        return;
    }
    verbose("message parsed!");
    successful_messages_parsed = successful_messages_parsed + 1;

    // Check checksum
    if (msg.expectedChecksum != msg.calculatedChecksum) {
        warning("checksum mismatch: %u != %u", msg.expectedChecksum, msg.calculatedChecksum);
        checksum_errors = checksum_errors + 1;
        return;
    }
    verbose("checksum valid!");

    // Find and invoke the handler for the message type
    for (size_t i = 0; i < sizeof(messageHandlers) / sizeof(MessageTypeHandler); ++i) {
        if (strcmp(msg.messageType, messageHandlers[i].messageType) == 0) {
            if (messageHandlers[i].handler(&msg)) {
                verbose("%s message handled!", messageHandlers[i].messageType);
                return;
            } else {
                warning("message handler failed!");
                return;
            }
        }
    }

    // Handle unknown message type
}