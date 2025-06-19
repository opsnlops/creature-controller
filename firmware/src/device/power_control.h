
#pragma once

#include "pico/stdlib.h"
#include "pico/stdio.h"

#include "controller/config.h"


#ifdef CC_VER3

// MARK: - Power control functions

/**
 * @brief Initialize power control pins for all motors
 *
 * Sets up GPIO pins for motor power control as outputs and ensures
 * all motors start in the powered-off state for safety.
 */
void init_motor_power_control(void);

/**
 * @brief Enable power to a specific motor
 *
 * @param motor_id The motor ID string (e.g., "0", "1", etc.)
 * @return true if power was enabled successfully, false if motor not found
 */
bool enable_motor_power(const char* motor_id);

/**
 * @brief Disable power to a specific motor
 *
 * @param motor_id The motor ID string (e.g., "0", "1", etc.)
 * @return true if power was disabled successfully, false if motor not found
 */
bool disable_motor_power(const char* motor_id);

/**
 * @brief Enable power to all motors
 *
 * @return true if all motors were powered successfully
 */
bool enable_all_motors(void);

/**
 * @brief Disable power to all motors (emergency stop)
 *
 * @return true if all motors were powered down successfully
 */
bool disable_all_motors(void);

/**
 * @brief Check if a motor has power enabled
 *
 * @param motor_id The motor ID string (e.g., "0", "1", etc.)
 * @return true if motor has power, false if not (or motor not found)
 */
bool is_motor_powered(const char* motor_id);

#endif