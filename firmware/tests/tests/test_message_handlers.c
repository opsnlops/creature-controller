/**
 * @file test_message_handlers.c
 * @brief Tests for handlePositionMessage and handleConfigMessage validation.
 *
 * These tests link the real message-handler sources and stub the controller
 * layer (see tests/mocks/controller_stubs.c). Compiled with CC_VER4=1 and
 * CC_VER3=1 so the Dynamixel branches are exercised.
 */

#include <string.h>

#include "unity.h"

#include "controller_stubs.h"
#include "dynamixel/dynamixel_registers.h"
#include "logging_mocks.h"
#include "messaging/messaging.h"
#include "messaging/processors/config_message.h"
#include "messaging/processors/position_message.h"
#include "types.h"

/* These are defined in controller_stubs.c. The handlers read/write them
   directly, and the tests assert on them. */
extern volatile bool controller_safe_to_run;
extern volatile u64 position_messages_processed;

static GenericMessage msg;

static void set_tokens(const char *type, const char *tokens[], int count) {
    memset(&msg, 0, sizeof(msg));
    strncpy(msg.messageType, type, MESSAGE_ACTION_MAX_SIZE - 1);
    for (int i = 0; i < count; i++) {
        strncpy(msg.tokens[i], tokens[i], MAX_TOKEN_LENGTH - 1);
    }
    msg.tokenCount = count;
}

void setUp(void) {
    reset_log_mocks();
    controller_stubs_reset();
    memset(&msg, 0, sizeof(msg));
}

void tearDown(void) {}

/* ---------------------------------------------------------------- POS happy paths */

void test_position_pwm_only_dispatches_each_token(void) {
    const char *tokens[] = {"B0 1500", "B1 1750"};
    set_tokens("POS", tokens, 2);

    TEST_ASSERT_TRUE(handlePositionMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(2, controller_stub_state.last_request_servo.call_count);
    /* Last call wins in the snapshot. */
    TEST_ASSERT_EQUAL_STRING("B1", controller_stub_state.last_request_servo.motor_id);
    TEST_ASSERT_EQUAL_UINT16(1750, controller_stub_state.last_request_servo.microseconds);
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_request_dxl.call_count);
    TEST_ASSERT_EQUAL_UINT32(1, controller_stub_state.first_frame_received_count);
}

void test_position_mixed_pwm_and_dxl_dispatches_both(void) {
    const char *tokens[] = {"B0 1500", "D1 2048", "D2 1024"};
    set_tokens("POS", tokens, 3);

    TEST_ASSERT_TRUE(handlePositionMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(1, controller_stub_state.last_request_servo.call_count);
    TEST_ASSERT_EQUAL_UINT32(2, controller_stub_state.last_request_dxl.call_count);
    TEST_ASSERT_EQUAL_UINT8(2, controller_stub_state.last_request_dxl.dxl_id);
    TEST_ASSERT_EQUAL_UINT32(1024, controller_stub_state.last_request_dxl.position);
}

void test_position_dxl_max_id_and_max_position_accepted(void) {
    const char *tokens[] = {"D253 4095"};
    set_tokens("POS", tokens, 1);

    TEST_ASSERT_TRUE(handlePositionMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(1, controller_stub_state.last_request_dxl.call_count);
    TEST_ASSERT_EQUAL_UINT8(DXL_MAX_ID, controller_stub_state.last_request_dxl.dxl_id);
    TEST_ASSERT_EQUAL_UINT32(DXL_POSITION_MAX, controller_stub_state.last_request_dxl.position);
}

void test_position_safety_gate_drops_silently(void) {
    const char *tokens[] = {"B0 1500"};
    set_tokens("POS", tokens, 1);
    controller_safe_to_run = false;

    TEST_ASSERT_TRUE(handlePositionMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_request_servo.call_count);
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.first_frame_received_count);
}

/* ---------------------------------------------------------------- POS rejections */

void test_position_rejects_empty_value(void) {
    const char *tokens[] = {"B0"};
    set_tokens("POS", tokens, 1);

    TEST_ASSERT_FALSE(handlePositionMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_request_servo.call_count);
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.first_frame_received_count);
}

void test_position_rejects_d_with_no_id(void) {
    const char *tokens[] = {"D 2048"};
    set_tokens("POS", tokens, 1);

    TEST_ASSERT_FALSE(handlePositionMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_request_dxl.call_count);
}

void test_position_rejects_dxl_id_zero(void) {
    const char *tokens[] = {"D0 2048"};
    set_tokens("POS", tokens, 1);

    TEST_ASSERT_FALSE(handlePositionMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_request_dxl.call_count);
}

void test_position_rejects_dxl_id_above_max(void) {
    const char *tokens[] = {"D254 2048"};
    set_tokens("POS", tokens, 1);

    TEST_ASSERT_FALSE(handlePositionMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_request_dxl.call_count);
}

void test_position_rejects_dxl_position_above_max(void) {
    const char *tokens[] = {"D1 4096"};
    set_tokens("POS", tokens, 1);

    TEST_ASSERT_FALSE(handlePositionMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_request_dxl.call_count);
}

void test_position_bad_token_aborts_whole_frame(void) {
    /* First token is fine; second is bad. The valid first dispatch happens
       before we discover the bad token, but the handler must still return
       false and must NOT count the frame as processed. */
    const char *tokens[] = {"B0 1500", "D0 2048"};
    set_tokens("POS", tokens, 2);
    const u64 before = position_messages_processed;

    TEST_ASSERT_FALSE(handlePositionMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(1, controller_stub_state.last_request_servo.call_count);
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_request_dxl.call_count);
    TEST_ASSERT_EQUAL_UINT64(before, position_messages_processed);
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.first_frame_received_count);
}

void test_position_request_servo_failure_aborts_frame(void) {
    const char *tokens[] = {"B0 1500", "B1 1750"};
    set_tokens("POS", tokens, 2);
    controller_stubs_set_request_servo_return(false);

    TEST_ASSERT_FALSE(handlePositionMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(1, controller_stub_state.last_request_servo.call_count);
}

/* ---------------------------------------------------------------- CONFIG happy paths */

void test_config_mixed_servo_and_dynamixel_configures_both(void) {
    const char *tokens[] = {"SERVO B0 1000 2000", "DYNAMIXEL 1 0 4095 100"};
    set_tokens("CONFIG", tokens, 2);

    TEST_ASSERT_TRUE(handleConfigMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(1, controller_stub_state.reset_servo_map_count);
    TEST_ASSERT_EQUAL_UINT32(1, controller_stub_state.reset_dxl_map_count);
    TEST_ASSERT_EQUAL_UINT32(1, controller_stub_state.last_configure_servo.call_count);
    TEST_ASSERT_EQUAL_STRING("B0", controller_stub_state.last_configure_servo.motor_id);
    TEST_ASSERT_EQUAL_UINT16(1000, controller_stub_state.last_configure_servo.min_us);
    TEST_ASSERT_EQUAL_UINT16(2000, controller_stub_state.last_configure_servo.max_us);
    TEST_ASSERT_EQUAL_UINT32(1, controller_stub_state.last_configure_dxl.call_count);
    TEST_ASSERT_EQUAL_UINT8(1, controller_stub_state.last_configure_dxl.dxl_id);
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_configure_dxl.min_pos);
    TEST_ASSERT_EQUAL_UINT32(DXL_POSITION_MAX, controller_stub_state.last_configure_dxl.max_pos);
    TEST_ASSERT_EQUAL_UINT32(100, controller_stub_state.last_configure_dxl.profile_velocity);
    TEST_ASSERT_EQUAL_UINT32(1, controller_stub_state.firmware_configuration_received_count);
}

void test_config_dxl_at_max_id_accepted(void) {
    const char *tokens[] = {"DYNAMIXEL 253 100 200 50"};
    set_tokens("CONFIG", tokens, 1);

    TEST_ASSERT_TRUE(handleConfigMessage(&msg));
    TEST_ASSERT_EQUAL_UINT8(DXL_MAX_ID, controller_stub_state.last_configure_dxl.dxl_id);
}

/* ---------------------------------------------------------------- CONFIG rejections */

void test_config_rejects_dxl_id_zero(void) {
    const char *tokens[] = {"DYNAMIXEL 0 100 200 50"};
    set_tokens("CONFIG", tokens, 1);

    TEST_ASSERT_FALSE(handleConfigMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_configure_dxl.call_count);
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.firmware_configuration_received_count);
}

void test_config_rejects_dxl_id_above_max(void) {
    const char *tokens[] = {"DYNAMIXEL 254 100 200 50"};
    set_tokens("CONFIG", tokens, 1);

    TEST_ASSERT_FALSE(handleConfigMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_configure_dxl.call_count);
}

void test_config_rejects_min_position_above_max_range(void) {
    const char *tokens[] = {"DYNAMIXEL 1 4096 4097 50"};
    set_tokens("CONFIG", tokens, 1);

    TEST_ASSERT_FALSE(handleConfigMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_configure_dxl.call_count);
}

void test_config_rejects_max_position_above_range(void) {
    const char *tokens[] = {"DYNAMIXEL 1 100 4096 50"};
    set_tokens("CONFIG", tokens, 1);

    TEST_ASSERT_FALSE(handleConfigMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_configure_dxl.call_count);
}

void test_config_rejects_min_greater_than_max(void) {
    const char *tokens[] = {"DYNAMIXEL 1 2000 1000 50"};
    set_tokens("CONFIG", tokens, 1);

    TEST_ASSERT_FALSE(handleConfigMessage(&msg));
    TEST_ASSERT_EQUAL_UINT32(0, controller_stub_state.last_configure_dxl.call_count);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_position_pwm_only_dispatches_each_token);
    RUN_TEST(test_position_mixed_pwm_and_dxl_dispatches_both);
    RUN_TEST(test_position_dxl_max_id_and_max_position_accepted);
    RUN_TEST(test_position_safety_gate_drops_silently);
    RUN_TEST(test_position_rejects_empty_value);
    RUN_TEST(test_position_rejects_d_with_no_id);
    RUN_TEST(test_position_rejects_dxl_id_zero);
    RUN_TEST(test_position_rejects_dxl_id_above_max);
    RUN_TEST(test_position_rejects_dxl_position_above_max);
    RUN_TEST(test_position_bad_token_aborts_whole_frame);
    RUN_TEST(test_position_request_servo_failure_aborts_frame);

    RUN_TEST(test_config_mixed_servo_and_dynamixel_configures_both);
    RUN_TEST(test_config_dxl_at_max_id_accepted);
    RUN_TEST(test_config_rejects_dxl_id_zero);
    RUN_TEST(test_config_rejects_dxl_id_above_max);
    RUN_TEST(test_config_rejects_min_position_above_max_range);
    RUN_TEST(test_config_rejects_max_position_above_range);
    RUN_TEST(test_config_rejects_min_greater_than_max);

    return UNITY_END();
}
