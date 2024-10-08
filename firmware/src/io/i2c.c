
#include "pico/stdlib.h"
#include "pico/stdio.h"

#include "device/mcp9808.h"
#include "device/pac1954.h"
#include "logging/logging.h"
#include "io/i2c.h"

#include "controller-config.h"
#include "types.h"

volatile bool i2c_setup_completed = false;

void setup_i2c() {
    debug("setting up i2c");

    // Set up the i2c controller to monitor our own sensors
    gpio_set_function(SENSORS_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SENSORS_I2C_SCL_PIN, GPIO_FUNC_I2C);

    i2c_init(SENSORS_I2C_BUS, SENSORS_I2C_FREQ);

    gpio_pull_up(SENSORS_I2C_SDA_PIN);
    gpio_pull_up(SENSORS_I2C_SCL_PIN);

    info("i2c has been set up! scl: %d, sda: %d", SENSORS_I2C_SCL_PIN, SENSORS_I2C_SDA_PIN);
    i2c_setup_completed = true;

    // Initialize the MCP9908
    init_mcp9808(SENSORS_I2C_BUS, I2C_DEVICE_MCP9808);

    // Initialize the PAC1954s
    init_pac1954(I2C_MOTOR0_PAC1954);
    init_pac1954(I2C_MOTOR1_PAC1954);
    init_pac1954(I2C_BOARD_PAC1954);

}