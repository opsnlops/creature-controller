#pragma once

#include "hardware/i2c.h"
#include "types.h"

/**
 * @brief Flag indicating if I2C setup has been completed successfully
 */
extern volatile bool i2c_setup_completed;

/**
 * @brief Set up the I2C peripherals and initialize connected devices
 *
 * Configures the I2C hardware with the pins and frequency defined in config.h.
 * Also initializes all connected I2C devices such as temperature sensors and
 * power monitors.
 *
 * @return true if I2C was successfully initialized, false otherwise
 */
bool setup_i2c(void);

/**
 * @brief Check if an I2C device is present at a specific address
 *
 * @param i2c The I2C instance to check (e.g., i2c0, i2c1)
 * @param addr The 7-bit I2C address to check
 * @return true if a device acknowledges at the address, false otherwise
 */
bool i2c_device_present(i2c_inst_t *i2c, uint8_t addr);