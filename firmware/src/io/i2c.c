
#include "pico/stdlib.h"
#include "pico/stdio.h"

#include "logging/logging.h"

#include "controller-config.h"

#include "io/i2c.h"

void setup_i2c() {
    debug("setting up i2c");

    // Set up the i2c controller that the servo boards use
    gpio_set_function(SENSORS_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SENSORS_I2C_SCL_PIN, GPIO_FUNC_I2C);

    i2c_init(SENSORS_I2C_BUS, SENSORS_I2C_FREQ);

    gpio_pull_up(SENSORS_I2C_SDA_PIN);
    gpio_pull_up(SENSORS_I2C_SCL_PIN);

    info("i2c for the servo controller has been set up");

}