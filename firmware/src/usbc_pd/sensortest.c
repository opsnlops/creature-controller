

#include <stddef.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "controller/config.h"
#include "types.h"

#include "device/pac1954.h"
#include "logging/logging.h"

#include "sensortest.h"

extern volatile bool i2c_setup_completed;

TimerHandle_t usbpd_i2c_sensor_read_timer = NULL;

void usbpd_sensor_init() {
    debug("setting up i2c");

    // Set up the i2c controller to monitor our own sensors
    gpio_set_function(SENSORS_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SENSORS_I2C_SCL_PIN, GPIO_FUNC_I2C);

    i2c_init(SENSORS_I2C_BUS, SENSORS_I2C_FREQ);

    gpio_pull_up(SENSORS_I2C_SDA_PIN);
    gpio_pull_up(SENSORS_I2C_SCL_PIN);

    info("i2c has been set up! scl: %d, sda: %d", SENSORS_I2C_SCL_PIN, SENSORS_I2C_SDA_PIN);
    i2c_setup_completed = true;

    // Initialize the PAC1954
    init_pac1954(I2C_USBCPD_PAC1954);

}

void usbpd_sensor_start() {
    debug("starting sensors");

    // Create the timer that reads the i2c sensors
    usbpd_i2c_sensor_read_timer = xTimerCreate(
            "I2C Sensor Read Timer",                    // Timer name
            pdMS_TO_TICKS(SENSOR_I2C_TIMER_TIME_MS),    // Fire every SENSOR_TIMER_TIME_MS
            pdTRUE,                                     // Auto-reload
            (void *) 0,                                 // Timer ID (not used here)
            usbpd_i2c_sensor_read_timer_callback        // Callback function
    );

    // Make sure this gets created
    configASSERT(usbpd_i2c_sensor_read_timer != NULL);

    // Start the timer
    xTimerStart(usbpd_i2c_sensor_read_timer, SENSOR_I2C_TIMER_TIME_MS / 2);

    info("started i2c sensor read timer");

}

void usbpd_i2c_sensor_read_timer_callback(TimerHandle_t xTimer) {

        // We don't use this on this timer
        (void) xTimer;

        // Read the power data for all of our sensors

        float voltage, power, current;
        const char *names[] = {"VBUS", " +5V", "PPHV", " 3v3"};

        for(int i = 0; i < 4; i++) {
            voltage = pac1954_read_voltage(I2C_USBCPD_PAC1954, i);
            current = pac1954_read_current(I2C_USBCPD_PAC1954, i);
            power = pac1954_read_power(I2C_USBCPD_PAC1954, i);
            debug("%s: %.2fv %.2fA %.2fw", names[i], voltage, current, power);
        }

        debug("-------");

        // Send refresh command to get fresh data for next pass
        pac1954_refresh(I2C_USBCPD_PAC1954);
}

