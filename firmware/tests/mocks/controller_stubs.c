/**
 * @file controller_stubs.c
 * @brief Stub implementations of the controller-side functions and globals
 *        referenced by handlePositionMessage / handleConfigMessage. Records
 *        call counts and last-call arguments for test assertions.
 */

#include <string.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "controller/controller.h"

#include "controller_stubs.h"
#include "types.h"

controller_stub_state_t controller_stub_state;

/* Globals referenced by the handlers via extern declarations. */
volatile bool controller_safe_to_run = true;
volatile bool has_first_frame_been_received = false;
volatile u64 position_messages_processed = 0;
TimerHandle_t controller_init_request_timer = NULL;
enum FirmwareState controller_firmware_state = idle;

void controller_stubs_reset(void) {
    memset(&controller_stub_state, 0, sizeof(controller_stub_state));
    controller_stub_state.last_request_servo.return_value = true;
    controller_stub_state.last_request_dxl.return_value = true;
    controller_stub_state.last_configure_servo.return_value = true;
    controller_stub_state.last_configure_dxl.return_value = true;
    controller_safe_to_run = true;
    has_first_frame_been_received = false;
    position_messages_processed = 0;
    controller_firmware_state = idle;
}

void controller_stubs_set_request_servo_return(bool value) {
    controller_stub_state.last_request_servo.return_value = value;
}

void controller_stubs_set_request_dxl_return(bool value) {
    controller_stub_state.last_request_dxl.return_value = value;
}

void controller_stubs_set_configure_servo_return(bool value) {
    controller_stub_state.last_configure_servo.return_value = value;
}

void controller_stubs_set_configure_dxl_return(bool value) {
    controller_stub_state.last_configure_dxl.return_value = value;
}

bool requestServoPosition(const char *motor_id, u16 requestedMicroseconds) {
    controller_stub_state.last_request_servo.call_count++;
    if (motor_id != NULL) {
        strncpy(controller_stub_state.last_request_servo.motor_id, motor_id,
                sizeof(controller_stub_state.last_request_servo.motor_id) - 1);
        controller_stub_state.last_request_servo
                .motor_id[sizeof(controller_stub_state.last_request_servo.motor_id) - 1] = '\0';
    } else {
        controller_stub_state.last_request_servo.motor_id[0] = '\0';
    }
    controller_stub_state.last_request_servo.microseconds = requestedMicroseconds;
    return controller_stub_state.last_request_servo.return_value;
}

bool requestDynamixelPosition(u8 dxl_id, u32 position) {
    controller_stub_state.last_request_dxl.call_count++;
    controller_stub_state.last_request_dxl.dxl_id = dxl_id;
    controller_stub_state.last_request_dxl.position = position;
    return controller_stub_state.last_request_dxl.return_value;
}

bool configureServoMinMax(const char *motor_id, u16 minMicroseconds, u16 maxMicroseconds) {
    controller_stub_state.last_configure_servo.call_count++;
    if (motor_id != NULL) {
        strncpy(controller_stub_state.last_configure_servo.motor_id, motor_id,
                sizeof(controller_stub_state.last_configure_servo.motor_id) - 1);
        controller_stub_state.last_configure_servo
                .motor_id[sizeof(controller_stub_state.last_configure_servo.motor_id) - 1] = '\0';
    } else {
        controller_stub_state.last_configure_servo.motor_id[0] = '\0';
    }
    controller_stub_state.last_configure_servo.min_us = minMicroseconds;
    controller_stub_state.last_configure_servo.max_us = maxMicroseconds;
    return controller_stub_state.last_configure_servo.return_value;
}

bool configureDynamixelServo(u8 dxl_id, u32 min_pos, u32 max_pos, u32 profile_velocity) {
    controller_stub_state.last_configure_dxl.call_count++;
    controller_stub_state.last_configure_dxl.dxl_id = dxl_id;
    controller_stub_state.last_configure_dxl.min_pos = min_pos;
    controller_stub_state.last_configure_dxl.max_pos = max_pos;
    controller_stub_state.last_configure_dxl.profile_velocity = profile_velocity;
    return controller_stub_state.last_configure_dxl.return_value;
}

void resetServoMotorMap(void) {
    controller_stub_state.reset_servo_map_count++;
}

void resetDynamixelMotorMap(void) {
    controller_stub_state.reset_dxl_map_count++;
}

void firmware_configuration_received(void) {
    controller_stub_state.firmware_configuration_received_count++;
}

void first_frame_received(bool yesOrNo) {
    controller_stub_state.first_frame_received_count++;
    controller_stub_state.last_first_frame_value = yesOrNo;
    has_first_frame_been_received = true;
}
