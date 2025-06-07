#include "pico/stdlib.h"
#include "pico/stdio.h"

#include "device/mcp9808.h"
#include "device/pac1954.h"
#include "logging/logging.h"
#include "io/i2c.h"

#include "controller/config.h"
#include "types.h"

/**
 * @brief Flag indicating if I2C setup has been completed successfully
 */
volatile bool i2c_setup_completed = false;

/**
 * @brief Set up the I2C peripherals and initialize connected devices
 *
 * @return true if I2C was successfully initialized, false otherwise
 */
bool setup_i2c(void) {
    debug("Setting up I2C...");

    // Configure the I2C pins
    gpio_set_function(SENSORS_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SENSORS_I2C_SCL_PIN, GPIO_FUNC_I2C);

    // Enable internal pull-ups for I2C pins
    gpio_pull_up(SENSORS_I2C_SDA_PIN);
    gpio_pull_up(SENSORS_I2C_SCL_PIN);

    // Initialize the I2C peripheral with the configured frequency
    i2c_init(SENSORS_I2C_BUS, SENSORS_I2C_FREQ);

    // Mark setup as complete and return success
    i2c_setup_completed = true;
    info("I2C initialized successfully (SCL: %d, SDA: %d, Freq: %d Hz)",
         SENSORS_I2C_SCL_PIN, SENSORS_I2C_SDA_PIN, SENSORS_I2C_FREQ);

    // At this point, we consider the I2C initialization successful
    // even if some devices are not found

    // Initialize the temperature sensor
    if (i2c_device_present(SENSORS_I2C_BUS, I2C_DEVICE_MCP9808)) {
        init_mcp9808(SENSORS_I2C_BUS, I2C_DEVICE_MCP9808);
        debug("MCP9808 temperature sensor initialized");
    } else {
        warning("MCP9808 temperature sensor not found at address 0x%02x", I2C_DEVICE_MCP9808);
    }


#ifdef CC_VER2

    // Initialize the power monitoring ICs
    if (i2c_device_present(SENSORS_I2C_BUS, I2C_MOTOR0_PAC1954)) {
        init_pac1954(I2C_MOTOR0_PAC1954);
        debug("Motor 0 PAC1954 power monitor initialized");
    } else {
        warning("Motor 0 PAC1954 not found at address 0x%02x", I2C_MOTOR0_PAC1954);
    }

    if (i2c_device_present(SENSORS_I2C_BUS, I2C_MOTOR1_PAC1954)) {
        init_pac1954(I2C_MOTOR1_PAC1954);
        debug("Motor 1 PAC1954 power monitor initialized");
    } else {
        warning("Motor 1 PAC1954 not found at address 0x%02x", I2C_MOTOR1_PAC1954);
    }

    if (i2c_device_present(SENSORS_I2C_BUS, I2C_BOARD_PAC1954)) {
        init_pac1954(I2C_BOARD_PAC1954);
        debug("Board PAC1954 power monitor initialized");
    } else {
        warning("Board PAC1954 not found at address 0x%02x", I2C_BOARD_PAC1954);
    }
#endif

    return true;
}

/**
 * @brief Check if an I2C device is present at a specific address
 *
 * Performs a quick transaction to see if a device ACKs at the specified address.
 *
 * @param i2c The I2C instance to check (e.g., i2c0, i2c1)
 * @param addr The 7-bit I2C address to check
 * @return true if a device acknowledges at the address, false otherwise
 */
bool i2c_device_present(i2c_inst_t *i2c, uint8_t addr) {
    // Try to read a single byte from the device
    uint8_t rxdata;
    int ret = i2c_read_blocking(i2c, addr, &rxdata, 1, false);

    // If ret >= 0, a device responded with an ACK
    return (ret >= 0);
}