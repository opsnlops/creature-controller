/**
 * @file controller_stubs.h
 * @brief Recordable stubs for controller-side functions called by the message
 *        handlers. Lets handler unit tests assert on what was called with what.
 */

#pragma once

#include <stdbool.h>

#include "types.h"

typedef struct {
    u32 call_count;
    char motor_id[16];
    u16 microseconds;
    bool return_value;
} request_servo_call_t;

typedef struct {
    u32 call_count;
    u8 dxl_id;
    u32 position;
    bool return_value;
} request_dxl_call_t;

typedef struct {
    u32 call_count;
    char motor_id[16];
    u16 min_us;
    u16 max_us;
    bool return_value;
} configure_servo_call_t;

typedef struct {
    u32 call_count;
    u8 dxl_id;
    u32 min_pos;
    u32 max_pos;
    u32 profile_velocity;
    bool return_value;
} configure_dxl_call_t;

typedef struct {
    u32 reset_servo_map_count;
    u32 reset_dxl_map_count;
    u32 firmware_configuration_received_count;
    u32 first_frame_received_count;
    bool last_first_frame_value;
    request_servo_call_t last_request_servo;
    request_dxl_call_t last_request_dxl;
    configure_servo_call_t last_configure_servo;
    configure_dxl_call_t last_configure_dxl;
} controller_stub_state_t;

extern controller_stub_state_t controller_stub_state;

/* Reset all counters and last-call records. Call from setUp(). */
void controller_stubs_reset(void);

/* Test helpers to control what the stubs return next. */
void controller_stubs_set_request_servo_return(bool value);
void controller_stubs_set_request_dxl_return(bool value);
void controller_stubs_set_configure_servo_return(bool value);
void controller_stubs_set_configure_dxl_return(bool value);
