/**
 * @file test_analog_filter.c
 * @brief Tests for responsive_analog_read_filter.c functions
 */

#include "unity.h"
#include "freertos_mocks.h"
#include "hardware_mocks.h"
#include "logging_mocks.h"
#include "io/responsive_analog_read_filter.h"
#include "types.h"

// Unity setup and teardown
void setUp(void) {
    // Reset mocks before each test
    reset_log_mocks();
}

void tearDown(void) {
    // Clean up after each test
}

// Test cases for analog filter creation and configuration
void test_create_analog_filter_default_values(void) {
    // Test creation with default values
    analog_filter filter = create_analog_filter(true, 0.1f, 20.0f, true);

    // Verify default values
    TEST_ASSERT_EQUAL_UINT16(0, filter.raw_value);
    TEST_ASSERT_EQUAL_UINT16(0, filter.responsive_value);
    TEST_ASSERT_EQUAL_UINT16(0, filter.previous_responsive_value);
    TEST_ASSERT_EQUAL_UINT16(4096, filter.analog_resolution);
    TEST_ASSERT_EQUAL_FLOAT(0.1f, filter.snap_multiplier);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, filter.activity_threshold);
    TEST_ASSERT_TRUE(filter.sleep_enable);
    TEST_ASSERT_TRUE(filter.edge_snap_enable);
    TEST_ASSERT_FALSE(filter.sleeping);
    TEST_ASSERT_FALSE(filter.responsive_value_has_changed);
}

void test_analog_filter_update_stable_input(void) {
    // Create filter with sleep disabled for predictable behavior
    analog_filter filter = create_analog_filter(false, 0.5f, 20.0f, false);

    // Single update with stable value
    analog_filter_update(&filter, 2000);

    // Values should match after first update since no smoothing occurs initially
    TEST_ASSERT_EQUAL_UINT16(2000, filter.raw_value);
    TEST_ASSERT_EQUAL_UINT16(2000, filter.responsive_value);
    TEST_ASSERT_TRUE(filter.responsive_value_has_changed);
}

void test_analog_filter_update_changing_input(void) {
    // Create filter with predictable parameters
    analog_filter filter = create_analog_filter(false, 0.5f, 20.0f, false);

    // Initial update
    analog_filter_update(&filter, 2000);
    TEST_ASSERT_EQUAL_UINT16(2000, filter.responsive_value);

    // Second update with significantly different value
    // With snap_multiplier=0.5, it should move partway toward the new value
    analog_filter_update(&filter, 3000);

    // Responsive value should be somewhere between 2000 and 3000
    TEST_ASSERT_EQUAL_UINT16(3000, filter.raw_value);
    TEST_ASSERT_GREATER_THAN(2000, filter.responsive_value);
    TEST_ASSERT_LESS_THAN(3000, filter.responsive_value);
    TEST_ASSERT_TRUE(filter.responsive_value_has_changed);
}

void test_analog_filter_sleep_threshold(void) {
    // Create filter with sleep enabled and large activity threshold
    analog_filter filter = create_analog_filter(true, 0.1f, 100.0f, false);

    // Initial update to establish baseline
    analog_filter_update(&filter, 2000);

    // Small changes should be smoothed and eventually lead to sleep
    for (int i = 0; i < 20; i++) {
        // Oscillate value within threshold range
        analog_filter_update(&filter, 2000 + (i % 2) * 10);
    }

    // After several small changes, filter should be sleeping
    TEST_ASSERT_TRUE(filter.sleeping);

    // Large change should wake up the filter
    analog_filter_update(&filter, 3000);
    TEST_ASSERT_FALSE(filter.sleeping);
}

void test_analog_filter_configuration_methods(void) {
    // Create basic filter
    analog_filter filter = create_analog_filter(false, 0.1f, 20.0f, false);

    // Test configuration methods
    analog_filter_set_snap_multiplier(&filter, 0.8f);
    TEST_ASSERT_EQUAL_FLOAT(0.8f, filter.snap_multiplier);

    analog_filter_enable_sleep(&filter);
    TEST_ASSERT_TRUE(filter.sleep_enable);

    analog_filter_disable_sleep(&filter);
    TEST_ASSERT_FALSE(filter.sleep_enable);

    analog_filter_enable_edge_snap(&filter);
    TEST_ASSERT_TRUE(filter.edge_snap_enable);

    analog_filter_disable_edge_snap(&filter);
    TEST_ASSERT_FALSE(filter.edge_snap_enable);

    analog_filter_set_activity_threshold(&filter, 50.0f);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, filter.activity_threshold);

    analog_filter_set_analog_resolution(&filter, 1024);
    TEST_ASSERT_EQUAL_UINT16(1024, filter.analog_resolution);
}

void test_snap_curve_function(void) {
    // Test the snap curve function at various points

    // Small input should give small output
    TEST_ASSERT_LESS_THAN(0.2f, analog_filter_snap_curve(0.1f));

    // Medium input should give medium output
    float medium_result = analog_filter_snap_curve(1.0f);
    TEST_ASSERT_GREATER_THAN(0.3f, medium_result);
    TEST_ASSERT_LESS_THAN(0.7f, medium_result);

    // Large input should give output close to 1.0
    TEST_ASSERT_GREATER_THAN(0.9f, analog_filter_snap_curve(10.0f));

    // Very large input should give exactly 1.0
    TEST_ASSERT_EQUAL_FLOAT(1.0f, analog_filter_snap_curve(100.0f));
}

void test_analog_filter_accessor_methods(void) {
    // Create filter and update it
    analog_filter filter = create_analog_filter(false, 0.5f, 20.0f, false);
    analog_filter_update(&filter, 2000);

    // Test getter methods
    TEST_ASSERT_EQUAL_UINT16(2000, analog_filter_get_raw_value(&filter));
    TEST_ASSERT_EQUAL_UINT16(2000, analog_filter_get_value(&filter));
    TEST_ASSERT_TRUE(analog_filter_has_changed(&filter));
    TEST_ASSERT_FALSE(analog_filter_is_sleeping(&filter));
}

int main(void) {
    UNITY_BEGIN();

    // Run tests for analog filter creation and configuration
    RUN_TEST(test_create_analog_filter_default_values);
    RUN_TEST(test_analog_filter_update_stable_input);
    RUN_TEST(test_analog_filter_update_changing_input);
    RUN_TEST(test_analog_filter_sleep_threshold);
    RUN_TEST(test_analog_filter_configuration_methods);
    RUN_TEST(test_snap_curve_function);
    RUN_TEST(test_analog_filter_accessor_methods);

    return UNITY_END();
}