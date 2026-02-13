/**
 * @file test_string_utils.c
 * @brief Tests for string_utils.c functions (minimalist version to debug segfaults)
 */

#include "unity.h"
#include "util/string_utils.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_to_binary_string(void) {
    printf("Running simple test_to_binary_string\n");

    const char *result = to_binary_string(0x55);
    printf("to_binary_string(0x55) returned: %s\n", result);

    // Test that result matches expected
    TEST_ASSERT_EQUAL_STRING("01010101", result);
}

int main(void) {
    printf("Starting minimal string_utils test\n");

    UNITY_BEGIN();
    RUN_TEST(test_to_binary_string);
    return UNITY_END();
}