
#pragma once

/**
 * @file dynamixel_servo.h
 * @brief High-level Dynamixel servo operations
 *
 * Provides convenient functions for interacting with Dynamixel servos.
 * Each function composes a packet and calls the HAL layer.
 */

#include <stdbool.h>

#include "dynamixel_hal.h"
#include "dynamixel_protocol.h"
#include "types.h"

/**
 * @brief Servo status snapshot
 */
typedef struct {
    u32 present_position;
    u8 present_temperature;
    u16 present_voltage;
    u16 present_load;
    u8 moving;
    u8 hardware_error;
} dxl_servo_status_t;

/**
 * @brief Ping result from a servo
 */
typedef struct {
    u16 model_number;
    u8 firmware_version;
} dxl_ping_result_t;

/**
 * @brief Callback for scan results
 *
 * @param id The servo ID that responded
 * @param model_number The servo model number
 * @param firmware_version The firmware version
 */
typedef void (*dxl_scan_callback_t)(u8 id, u16 model_number, u8 firmware_version);

/**
 * @brief Default timeout for normal operations (ms)
 */
#define DXL_DEFAULT_TIMEOUT_MS 100

/**
 * @brief Short timeout used during scan operations (ms)
 */
#define DXL_SCAN_TIMEOUT_MS 10

/**
 * @brief Ping a servo and retrieve its model number and firmware version
 */
dxl_result_t dxl_ping(dxl_hal_context_t *ctx, u8 id, dxl_ping_result_t *result);

/**
 * @brief Scan for servos on the bus
 *
 * Iterates through servo IDs and reports found servos via callback.
 *
 * @param ctx HAL context
 * @param start_id First ID to scan (inclusive)
 * @param end_id Last ID to scan (inclusive)
 * @param callback Function called for each servo found
 */
void dxl_scan(dxl_hal_context_t *ctx, u8 start_id, u8 end_id, dxl_scan_callback_t callback);

/**
 * @brief Read a register from a servo
 *
 * @param ctx HAL context
 * @param id Servo ID
 * @param address Register address
 * @param length Number of bytes to read (1, 2, or 4)
 * @param value Output value (little-endian)
 * @return DXL_OK on success
 */
dxl_result_t dxl_read_register(dxl_hal_context_t *ctx, u8 id, u16 address, u16 length, u32 *value);

/**
 * @brief Write a register on a servo
 *
 * @param ctx HAL context
 * @param id Servo ID
 * @param address Register address
 * @param length Number of bytes to write (1, 2, or 4)
 * @param value Value to write (little-endian)
 * @return DXL_OK on success
 */
dxl_result_t dxl_write_register(dxl_hal_context_t *ctx, u8 id, u16 address, u16 length, u32 value);

/**
 * @brief Change a servo's ID
 */
dxl_result_t dxl_set_id(dxl_hal_context_t *ctx, u8 current_id, u8 new_id);

/**
 * @brief Set the servo's baud rate (by Dynamixel baud index 0-7)
 */
dxl_result_t dxl_set_baud_rate(dxl_hal_context_t *ctx, u8 id, u8 baud_index);

/**
 * @brief Factory reset a servo
 *
 * @param ctx HAL context
 * @param id Servo ID
 * @param option DXL_RESET_ALL, DXL_RESET_EXCEPT_ID, or DXL_RESET_EXCEPT_ID_BAUD
 */
dxl_result_t dxl_factory_reset(dxl_hal_context_t *ctx, u8 id, u8 option);

/**
 * @brief Reboot a servo
 */
dxl_result_t dxl_reboot(dxl_hal_context_t *ctx, u8 id);

/**
 * @brief Enable or disable torque
 */
dxl_result_t dxl_set_torque(dxl_hal_context_t *ctx, u8 id, bool enable);

/**
 * @brief Set goal position (0-4095)
 */
dxl_result_t dxl_set_position(dxl_hal_context_t *ctx, u8 id, u32 position);

/**
 * @brief Turn the servo's LED on or off
 */
dxl_result_t dxl_set_led(dxl_hal_context_t *ctx, u8 id, bool on);

/**
 * @brief Set the profile velocity
 */
dxl_result_t dxl_set_profile_velocity(dxl_hal_context_t *ctx, u8 id, u32 velocity);

/**
 * @brief Read comprehensive status from a servo
 */
dxl_result_t dxl_read_status(dxl_hal_context_t *ctx, u8 id, dxl_servo_status_t *status);

/**
 * @brief Convert a Dynamixel baud rate index (0-7) to the actual baud rate value
 *
 * @param index Baud rate index from the servo's register
 * @return Actual baud rate in bps, or 0 if invalid
 */
u32 dxl_baud_index_to_rate(u8 index);
