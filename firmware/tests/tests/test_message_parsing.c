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

// Unity setup and teardown
void setUp(void) {
    // Reset mocks before each test
    reset_log_mocks();
}

void tearDown(void) {
    // Clean up after each test
}

// Helper functions
u16 calculate_test_checksum(const char *message) {
    return calculateChecksum(message);
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
    // PING + 1234 = 'P'(80) + 'I'(73) + 'N'(78) + 'G'(71) + 1234 = 328 (including the tab)
    const char *test_message = "PING\t1234\tCHK 328";
    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Verify parsing succeeded
    TEST_ASSERT_TRUE(result);

    // Verify message type
    TEST_ASSERT_EQUAL_STRING("PING", msg.messageType);

    // Verify token count (1 token, not counting checksum)
    TEST_ASSERT_EQUAL_INT(1, msg.tokenCount);

    // Verify token content
    TEST_ASSERT_EQUAL_STRING("1234", msg.tokens[0]);

    // Verify checksum
    TEST_ASSERT_EQUAL_UINT16(328, msg.expectedChecksum);
    TEST_ASSERT_EQUAL_UINT16(328, msg.calculatedChecksum);
}

void test_parseMessage_multiple_tokens(void) {
    // Create a message with multiple tokens
    const char *test_message = "POS\t0 1500\t1 1750\t2 2000\tCHK 692";
    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

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

    // Verify checksum
    TEST_ASSERT_EQUAL_UINT16(692, msg.expectedChecksum);
    TEST_ASSERT_EQUAL_UINT16(692, msg.calculatedChecksum);
}

void test_parseMessage_invalid_checksum(void) {
    // Create a message with invalid checksum format
    const char *test_message = "PING\t1234\tBadChecksum";
    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Verify parsing failed
    TEST_ASSERT_FALSE(result);
}

void test_parseMessage_checksum_mismatch(void) {
    // Create a message with mismatched checksum
    const char *test_message = "PING\t1234\tCHK 999"; // Correct checksum would be 328
    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Verify parsing succeeded but checksums don't match
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT16(999, msg.expectedChecksum);
    TEST_ASSERT_EQUAL_UINT16(328, msg.calculatedChecksum);
}

void test_parseMessage_too_few_tokens(void) {
    // Create a message with no data tokens
    const char *test_message = "PING\tCHK 0";
    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Should fail because at least messageType and checksum are needed
    TEST_ASSERT_FALSE(result);
}

void test_parseMessage_no_tabs(void) {
    // Create a message with no tab separators
    const char *test_message = "PING 1234 CHK 328";
    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Should fail because tokens are tab-separated
    TEST_ASSERT_FALSE(result);
}

void test_parseMessage_max_tokens(void) {
    // Create a message with the maximum number of tokens
    char test_message[1024] = "MAX";

    // Add MAX_TOKENS-1 tokens (leaving room for checksum)
    for (int i = 0; i < MAX_TOKENS-1; i++) {
        char token[20];
        sprintf(token, "\ttoken%d", i);
        strcat(test_message, token);
    }

    // Add checksum (using a dummy value)
    strcat(test_message, "\tCHK 999");

    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Verify parsing succeeded
    TEST_ASSERT_TRUE(result);

    // Verify token count (MAX_TOKENS-1, not counting checksum)
    TEST_ASSERT_EQUAL_INT(MAX_TOKENS-1, msg.tokenCount);
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
    RUN_TEST(test_parseMessage_checksum_mismatch);
    RUN_TEST(test_parseMessage_too_few_tokens);
    RUN_TEST(test_parseMessage_no_tabs);
    RUN_TEST(test_parseMessage_max_tokens);

    return UNITY_END();
}/**
 * @file test_message_parsing.c
 * @brief Tests for messaging.c parsing functions
 */

#include "unity.h"
#include "freertos_mocks.h"
#include "hardware_mocks.h"
#include "logging_mocks.h"
#include "messaging/messaging.h"
#include "types.h"

// Unity setup and teardown
void setUp(void) {
    // Reset mocks before each test
    reset_log_mocks();
}

void tearDown(void) {
    // Clean up after each test
}

// Helper functions
u16 calculate_test_checksum(const char *message) {
    return calculateChecksum(message);
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
    // PING + 1234 = 'P'(80) + 'I'(73) + 'N'(78) + 'G'(71) + 1234 = 328 (including the tab)
    const char *test_message = "PING\t1234\tCHK 328";
    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Verify parsing succeeded
    TEST_ASSERT(result);

    // Verify message type
    TEST_ASSERT_EQUAL_STRING("PING", msg.messageType);

    // Verify token count (1 token, not counting checksum)
    TEST_ASSERT_EQUAL_INT(1, msg.tokenCount);

    // Verify token content
    TEST_ASSERT_EQUAL_STRING("1234", msg.tokens[0]);

    // Verify checksum
    TEST_ASSERT_EQUAL_UINT16(328, msg.expectedChecksum);
    TEST_ASSERT_EQUAL_UINT16(328, msg.calculatedChecksum);
}

void test_parseMessage_multiple_tokens(void) {
    // Create a message with multiple tokens
    const char *test_message = "POS\t0 1500\t1 1750\t2 2000\tCHK 692";
    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Verify parsing succeeded
    TEST_ASSERT(result);

    // Verify message type
    TEST_ASSERT_EQUAL_STRING("POS", msg.messageType);

    // Verify token count (3 tokens, not counting checksum)
    TEST_ASSERT_EQUAL_INT(3, msg.tokenCount);

    // Verify token content
    TEST_ASSERT_EQUAL_STRING("0 1500", msg.tokens[0]);
    TEST_ASSERT_EQUAL_STRING("1 1750", msg.tokens[1]);
    TEST_ASSERT_EQUAL_STRING("2 2000", msg.tokens[2]);

    // Verify checksum
    TEST_ASSERT_EQUAL_UINT16(692, msg.expectedChecksum);
    TEST_ASSERT_EQUAL_UINT16(692, msg.calculatedChecksum);
}

void test_parseMessage_invalid_checksum(void) {
    // Create a message with invalid checksum format
    const char *test_message = "PING\t1234\tBadChecksum";
    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Verify parsing failed
    TEST_ASSERT(!result);
}

void test_parseMessage_checksum_mismatch(void) {
    // Create a message with mismatched checksum
    const char *test_message = "PING\t1234\tCHK 999"; // Correct checksum would be 328
    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Verify parsing succeeded but checksums don't match
    TEST_ASSERT(result);
    TEST_ASSERT_EQUAL_UINT16(999, msg.expectedChecksum);
    TEST_ASSERT_EQUAL_UINT16(328, msg.calculatedChecksum);
}

void test_parseMessage_too_few_tokens(void) {
    // Create a message with no data tokens
    const char *test_message = "PING\tCHK 0";
    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Should fail because at least messageType and checksum are needed
    TEST_ASSERT(!result);
}

void test_parseMessage_no_tabs(void) {
    // Create a message with no tab separators
    const char *test_message = "PING 1234 CHK 328";
    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Should fail because tokens are tab-separated
    TEST_ASSERT(!result);
}

void test_parseMessage_max_tokens(void) {
    // Create a message with the maximum number of tokens
    char test_message[1024] = "MAX";

    // Add MAX_TOKENS-1 tokens (leaving room for checksum)
    for (int i = 0; i < MAX_TOKENS-1; i++) {
        char token[20];
        sprintf(token, "\ttoken%d", i);
        strcat(test_message, token);
    }

    // Add checksum (using a dummy value)
    strcat(test_message, "\tCHK 999");

    GenericMessage msg;

    // Parse message
    bool result = parseMessage(test_message, &msg);

    // Verify parsing succeeded
    TEST_ASSERT(result);

    // Verify token count (MAX_TOKENS-1, not counting checksum)
    TEST_ASSERT_EQUAL_INT(MAX_TOKENS-1, msg.tokenCount);
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
    RUN_TEST(test_parseMessage_checksum_mismatch);
    RUN_TEST(test_parseMessage_too_few_tokens);
    RUN_TEST(test_parseMessage_no_tabs);
    RUN_TEST(test_parseMessage_max_tokens);

    return UNITY_END();
}