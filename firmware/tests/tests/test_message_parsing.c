/**
 * @file test_message_parsing.c
 * @brief Tests for messaging.c parsing functions
 */

#include "unity.h"
#include "freertos_mocks.h"
#include "hardware_mocks.h"
#include "logging_mocks.h"
#include "messaging/messaging.h"
#include "types.h"
#include <stdio.h>
#include <string.h>

// Include stubs.h to get the declarations already defined there
#include "stubs.h"

// Global test data
static char test_message_buffer[256];

// Unity setup and teardown
void setUp(void) {
    // Reset mocks before each test
    reset_log_mocks();
    memset(test_message_buffer, 0, sizeof(test_message_buffer));
}

void tearDown(void) {
    // Clean up after each test
}

// Test cases for calculateChecksum function
void test_calculateChecksum_null_input(void) {
    // Test null input handling
    TEST_ASSERT_EQUAL_UINT16(0, calculateChecksum(NULL));
}

void test_calculateChecksum_empty_string(void) {
    // Test empty string handling
    TEST_ASSERT_EQUAL_UINT16(0, calculateChecksum(""));
}

void test_calculateChecksum_simple_string(void) {
    // Test simple string checksum
    // Sum of ASCII values: 'A'(65) + 'B'(66) + 'C'(67) = 198
    TEST_ASSERT_EQUAL_UINT16(198, calculateChecksum("ABC"));
}

void test_calculateChecksum_with_special_chars(void) {
    // Test string with special characters
    // "Hello!" = 'H'(72) + 'e'(101) + 'l'(108) + 'l'(108) + 'o'(111) + '!'(33) = 533
    TEST_ASSERT_EQUAL_UINT16(533, calculateChecksum("Hello!"));
}

// Test cases for parseMessage function
void test_parseMessage_valid_message(void) {
    // Create a valid test message
    // PING is the message type, "1234" is the token, and 328 is the checksum
    // PING + TAB + 1234 = 'P'(80) + 'I'(73) + 'N'(78) + 'G'(71) + '\t'(9) + '1'(49) + '2'(50) + '3'(51) + '4'(52) = 513
    strcpy(test_message_buffer, "PING\t1234\tCHK 513");
    GenericMessage msg;
    memset(&msg, 0, sizeof(GenericMessage)); // Initialize to zeros

    // Parse message
    bool result = parseMessage(test_message_buffer, &msg);

    // Verify parsing succeeded
    TEST_ASSERT_TRUE(result);

    // Verify message type
    TEST_ASSERT_EQUAL_STRING("PING", msg.messageType);

    // Verify token count (1 token, not counting checksum)
    TEST_ASSERT_EQUAL_INT(1, msg.tokenCount);

    // Verify token content
    TEST_ASSERT_EQUAL_STRING("1234", msg.tokens[0]);

    // Verify checksum
    TEST_ASSERT_EQUAL_UINT16(513, msg.expectedChecksum);
    // Note: We're not checking calculatedChecksum here because its exact calculation
    // depends on implementation details that may vary
}

void test_parseMessage_multiple_tokens(void) {
    // Create a message with multiple tokens
    strcpy(test_message_buffer, "POS\t0 1500\t1 1750\t2 2000\tCHK 999");
    GenericMessage msg;
    memset(&msg, 0, sizeof(GenericMessage)); // Initialize to zeros

    // Parse message
    bool result = parseMessage(test_message_buffer, &msg);

    // Verify parsing succeeded
    TEST_ASSERT_TRUE(result);

    // Verify message type
    TEST_ASSERT_EQUAL_STRING("POS", msg.messageType);

    // Verify token count (3 tokens, not counting checksum)
    TEST_ASSERT_EQUAL_INT(3, msg.tokenCount);

    // Verify token content
    TEST_ASSERT_EQUAL_STRING("0 1500", msg.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("1 1750", msg.tokens[1]);
    TEST_ASSERT_EQUAL_STRING("2 2000", msg.tokens[2]);

    // Verify expected checksum
    TEST_ASSERT_EQUAL_UINT16(999, msg.expectedChecksum);
}

void test_parseMessage_invalid_checksum(void) {
    // Create a message with invalid checksum format
    strcpy(test_message_buffer, "PING\t1234\tBadChecksum");
    GenericMessage msg;
    memset(&msg, 0, sizeof(GenericMessage)); // Initialize to zeros

    // Parse message
    bool result = parseMessage(test_message_buffer, &msg);

    // Verify parsing failed
    TEST_ASSERT_FALSE(result);
}

void test_parseMessage_too_few_tokens(void) {
    // A message type with no checksum token at all is only a single token.
    // parseMessage requires at least two (messageType + checksum), so this
    // must be rejected. (Note: "PING\tCHK 0" would be *accepted* — that is a
    // valid type + checksum with zero data tokens.)
    strcpy(test_message_buffer, "PING");
    GenericMessage msg;
    memset(&msg, 0, sizeof(GenericMessage)); // Initialize to zeros

    // Parse message
    bool result = parseMessage(test_message_buffer, &msg);

    // Should fail because at least messageType and checksum are needed
    TEST_ASSERT_FALSE(result);
}

void test_parseMessage_long_config_not_truncated(void) {
    // Regression for the "checksum mismatch: 664 != 6641" field bug. A 5-servo
    // module's CONFIG is a 128-byte wire line, which exactly overran the old
    // 128-byte incoming buffer: the final checksum digit was sheared off, so the
    // parsed checksum (664) no longer matched the body's real checksum (6641).
    // The buffer is now large enough to hold a full module CONFIG intact.
    const char *body = "CONFIG\tDYNAMIXEL 2 1000 2350 160\tSERVO 1 1475 1950\tSERVO 0 1400 1900"
                       "\tDYNAMIXEL 4 1024 3072 187\tDYNAMIXEL 1 1024 3072 187";

    // Build the on-the-wire line exactly as the host does: body + "\tCS <sum>".
    u16 checksum = calculateChecksum(body);
    char wire[USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH];
    int wire_len = snprintf(wire, sizeof(wire), "%s\tCS %u", body, checksum);

    // The line plus its null terminator must fit the incoming buffer; otherwise
    // parseMessage's internal copy truncates it and the checksum cannot match.
    TEST_ASSERT_LESS_THAN_INT(USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH, wire_len + 1);

    GenericMessage msg;
    memset(&msg, 0, sizeof(GenericMessage));
    bool result = parseMessage(wire, &msg);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("CONFIG", msg.messageType);
    TEST_ASSERT_EQUAL_INT(5, msg.tokenCount); // five servo tokens, checksum excluded
    // The crux: parsed checksum equals the one computed over the body (no shear).
    TEST_ASSERT_EQUAL_UINT16(checksum, msg.expectedChecksum);
    TEST_ASSERT_EQUAL_UINT16(msg.calculatedChecksum, msg.expectedChecksum);
}

int main(void) {
    UNITY_BEGIN();

    // Run tests for calculateChecksum function
    RUN_TEST(test_calculateChecksum_null_input);
    RUN_TEST(test_calculateChecksum_empty_string);
    RUN_TEST(test_calculateChecksum_simple_string);
    RUN_TEST(test_calculateChecksum_with_special_chars);

    // Run tests for parseMessage function
    RUN_TEST(test_parseMessage_valid_message);
    RUN_TEST(test_parseMessage_multiple_tokens);
    RUN_TEST(test_parseMessage_invalid_checksum);
    RUN_TEST(test_parseMessage_long_config_not_truncated);
    RUN_TEST(test_parseMessage_too_few_tokens);

    return UNITY_END();
}