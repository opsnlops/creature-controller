
#pragma once

#include <stdint.h>

#include "controller-config.h"


// Assume MAX_TOKENS is the maximum number of tokens you expect in a message
#define MAX_TOKENS 30

// MAX_TOKEN_LENGTH is the maximum length of each token
#define MAX_TOKEN_LENGTH 40

// The size of a message action
#define MESSAGE_ACTION_MAX_SIZE 8

/**
 * A generic messaging from the controller
 */
typedef struct {
    char messageType[MESSAGE_ACTION_MAX_SIZE];
    char tokens[MAX_TOKENS][MAX_TOKEN_LENGTH];
    int tokenCount;
    u16 expectedChecksum;
    u16 calculatedChecksum;
} GenericMessage;

/**
 * Our Message Handler
 */
typedef bool (*MessageHandler)(const GenericMessage*);

typedef struct {
    char messageType[MESSAGE_ACTION_MAX_SIZE];
    MessageHandler handler;
} MessageTypeHandler;


/**
 * Process an incoming message from the serial connection
 *
 * @param rawMessage the raw message
 */
void processMessage(const char* rawMessage);

/**
 * Parse a message from the serial connection
 *
 * @param rawMessage raw message in
 * @param outMessage GenericMessage out
 * @return true if it worked, false if not
 */
bool parseMessage(const char* rawMessage, GenericMessage* outMessage);


/**
 * Calculate the checksum of a message
 *
 * @param message the message to check
 * @return the u16 checksum
 */
u16 calculateChecksum(const char* message);
