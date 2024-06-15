
#include "pico/stdlib.h"
#include "pico/stdio.h"

#include "logging/logging.h"

#include "controller-config.h"

#include "io/i2c.h"


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

}