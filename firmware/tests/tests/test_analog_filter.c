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

    // Initialize to ensure smooth_value is not garbage
    filter.smooth_value = 0.0f;

    // Single update with stable value
    analog_filter_update(&filter, 2000);

    // Values should match after first update since no smoothing occurs initially
    TEST_ASSERT_EQUAL_UINT16(2000, filter.raw_value);
    TEST_ASSERT_EQUAL_UINT16(2000, filter.responsive_value);
    TEST_ASSERT_TRUE(filter.responsive_value_has_changed);
}

void test_analog_filter_update_changing_input(void) {
    // The snap curve saturates (snap == 1.0, i.e. a full jump to the new value)
    // once the scaled input difference reaches 1, where scaled input is
    // diff * snap_multiplier. To exercise *partial* smoothing for a 1000-count
    // jump we need diff * snap_multiplier < 1, so use a small snap_multiplier.
    // (In production big swings are meant to snap fully; this test targets the
    // smoothing region on purpose.)
    analog_filter filter = create_analog_filter(false, 0.0005f, 20.0f, false);

    // Initialize to ensure smooth_value is not garbage
    filter.smooth_value = 0.0f;

    // Initial update: diff = 2000, scaled input = 1.0, so this snaps fully.
    analog_filter_update(&filter, 2000);
    TEST_ASSERT_EQUAL_UINT16(2000, filter.responsive_value);

    // Second update: diff = 1000, scaled input = 0.5, so it moves only partway.
    analog_filter_update(&filter, 3000);

    // Responsive value should be somewhere between 2000 and 3000
    TEST_ASSERT_EQUAL_UINT16(3000, filter.raw_value);
    TEST_ASSERT_GREATER_THAN_UINT16(2000, filter.responsive_value);
    TEST_ASSERT_LESS_THAN_UINT16(3000, filter.responsive_value);
    TEST_ASSERT_TRUE(filter.responsive_value_has_changed);
}

void test_analog_filter_sleep_threshold(void) {
    // Create filter with sleep enabled and large activity threshold
    analog_filter filter = create_analog_filter(true, 0.1f, 100.0f, false);

    // Initialize to ensure smooth_value and error_ema are not garbage
    filter.smooth_value = 0.0f;
    filter.error_ema = 0.0f;

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
    // The snap curve maps a (scaled) input difference to a 0..1 smoothing
    // factor: f(x) = min(1, (1 - 1/(x + 1)) * 2). It rises steeply and
    // saturates at exactly 1.0 for all x >= 1.
    //
    // NOTE: Unity's TEST_ASSERT_LESS_THAN / GREATER_THAN are integer
    // comparisons, so they cannot be used to check these fractional results
    // (the arguments would be truncated to 0). Use float-aware assertions.

    // Zero input means no movement.
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, analog_filter_snap_curve(0.0f));

    // Small input gives a small (but non-zero) output.
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.18182f, analog_filter_snap_curve(0.1f));

    // A mid-range input (below saturation) gives a mid-range output.
    float medium_result = analog_filter_snap_curve(0.4f);
    TEST_ASSERT_TRUE(medium_result > 0.3f);
    TEST_ASSERT_TRUE(medium_result < 0.7f);

    // The curve saturates at exactly 1.0 once x reaches 1.
    TEST_ASSERT_EQUAL_FLOAT(1.0f, analog_filter_snap_curve(1.0f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, analog_filter_snap_curve(10.0f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, analog_filter_snap_curve(100.0f));
}

void test_analog_filter_accessor_methods(void) {
    // Create filter and update it
    analog_filter filter = create_analog_filter(false, 0.5f, 20.0f, false);

    // Initialize to ensure smooth_value is not garbage
    filter.smooth_value = 0.0f;

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