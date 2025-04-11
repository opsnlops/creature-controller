/**
 * @file test_string_utils.c
 * @brief Tests for string_utils.c functions
 */

#include <string.h>
#include <stdio.h>

#include "unity.h"
#include "freertos_mocks.h"
#include "hardware_mocks.h"
#include "logging_mocks.h"
#include "util/string_utils.h"
#include "types.h"

// Unity setup and teardown
void setUp(void) {
    // Reset mocks before each test
    reset_log_mocks();
}

void tearDown(void) {
    // Clean up after each test
}

// Test cases for stringToU16 function
void test_stringToU16_null_input(void) {
    // Test null input handling
    TEST_ASSERT_EQUAL_UINT16(0, stringToU16(NULL));
}

void test_stringToU16_empty_string(void) {
    // Test empty string handling
    TEST_ASSERT_EQUAL_UINT16(0, stringToU16(""));
}

void test_stringToU16_whitespace_only(void) {
    // Test whitespace handling
    TEST_ASSERT_EQUAL_UINT16(0, stringToU16("   "));
}

void test_stringToU16_decimal_value(void) {
    // Test normal decimal input
    TEST_ASSERT_EQUAL_UINT16(123, stringToU16("123"));
}

void test_stringToU16_hex_value_lowercase(void) {
    // Test hexadecimal input (lowercase)
    TEST_ASSERT_EQUAL_UINT16(0xabc, stringToU16("0xabc"));
}

void test_stringToU16_hex_value_uppercase(void) {
    // Test hexadecimal input (uppercase)
    TEST_ASSERT_EQUAL_UINT16(0xABC, stringToU16("0xABC"));
}

void test_stringToU16_leading_whitespace(void) {
    // Test handling of leading whitespace
    TEST_ASSERT_EQUAL_UINT16(42, stringToU16("  42"));
}

void test_stringToU16_trailing_characters(void) {
    // Test handling of trailing non-numeric characters
    TEST_ASSERT_EQUAL_UINT16(123, stringToU16("123abc"));
}

void test_stringToU16_invalid_input(void) {
    // Test invalid input handling
    TEST_ASSERT_EQUAL_UINT16(0, stringToU16("abc"));

    // Verify warning log was generated
    TEST_ASSERT_TRUE(log_contains("Failed to convert string to u16"));
}

void test_stringToU16_overflow(void) {
    // Test overflow handling (UINT16_MAX = 65535)
    TEST_ASSERT_EQUAL_UINT16(0, stringToU16("70000"));

    // Verify warning log was generated
    TEST_ASSERT_TRUE(log_contains("Failed to convert string to u16"));
}

// Test cases for to_binary_string function
void test_to_binary_string_zero(void) {
    // Test conversion of 0
    TEST_ASSERT_EQUAL_STRING("00000000", to_binary_string(0));
}

void test_to_binary_string_one(void) {
    // Test conversion of 1
    TEST_ASSERT_EQUAL_STRING("00000001", to_binary_string(1));
}

void test_to_binary_string_max(void) {
    // Test conversion of 255 (0xFF)
    TEST_ASSERT_EQUAL_STRING("11111111", to_binary_string(255));
}

void test_to_binary_string_mixed(void) {
    // Test conversion of mixed bit pattern (0xA5 = 10100101)
    TEST_ASSERT_EQUAL_STRING("10100101", to_binary_string(0xA5));
}

int main(void) {
    UNITY_BEGIN();

    // Run tests for stringToU16 function
    RUN_TEST(test_stringToU16_null_input);
    RUN_TEST(test_stringToU16_empty_string);
    RUN_TEST(test_stringToU16_whitespace_only);
    RUN_TEST(test_stringToU16_decimal_value);
    RUN_TEST(test_stringToU16_hex_value_lowercase);
    RUN_TEST(test_stringToU16_hex_value_uppercase);
    RUN_TEST(test_stringToU16_leading_whitespace);
    RUN_TEST(test_stringToU16_trailing_characters);
    RUN_TEST(test_stringToU16_invalid_input);
    RUN_TEST(test_stringToU16_overflow);

    // Run tests for to_binary_string function
    RUN_TEST(test_to_binary_string_zero);
    RUN_TEST(test_to_binary_string_one);
    RUN_TEST(test_to_binary_string_max);
    RUN_TEST(test_to_binary_string_mixed);

    return UNITY_END();
}