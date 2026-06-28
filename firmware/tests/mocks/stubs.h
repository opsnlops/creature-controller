#pragma once

/**
 * @file stubs.h
 * @brief Common stubs and definitions for test mocks
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Include the real types.h from the firmware
#include "types.h"

// We'll need to include messaging.h before using GenericMessage
// So instead of forward declaring, we'll just declare function prototypes
// with void* and let the compiler handle conversion

// Function declarations for message processors that need to be mocked
// These will be implemented in message_handlers_mock.c. Tests that link the
// REAL handlers must define NO_AUTOSTUB_MESSAGING_HANDLERS to avoid the
// conflicting `const void *` prototypes here clashing with the real headers.
#ifndef NO_AUTOSTUB_MESSAGING_HANDLERS
bool handlePingMessage(const void *msg);
bool handlePositionMessage(const void *msg);
bool handleConfigMessage(const void *msg);
bool handleEmergencyStopMessage(const void *msg);
#endif

// Define values for config.h that might be needed for tests
#ifndef LOGGING_MESSAGE_MAX_LENGTH
#define LOGGING_MESSAGE_MAX_LENGTH 256
#endif

#ifndef USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH
#define USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH 384
#endif

#ifndef UART_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH
#define UART_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH 255
#endif

#ifndef OUTGOING_MESSAGE_MAX_LENGTH
#define OUTGOING_MESSAGE_MAX_LENGTH 384
#endif

// Fallback for test TUs that don't pull in controller/config.h. Keep this in
// sync with config.h's value so a TU that includes both doesn't trip
// -Wmacro-redefined.
#ifndef INCOMING_MESSAGE_MAX_LENGTH
#define INCOMING_MESSAGE_MAX_LENGTH 256
#endif

// Define TEST_BUILD to allow conditional compilation in the firmware code
#define TEST_BUILD 1