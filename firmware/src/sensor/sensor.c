
#include <stddef.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "controller-config.h"

#include "controller/controller.h"
#include "device/mcp3304.h"
#include "device/mcp9808.h"
#include "device/pac1954.h"
#include "io/responsive_analog_read_filter.h"
#include "logging/logging.h"

#include "sensor.h"


// From i2c.c
extern volatile bool i2c_setup_completed;

// From spi.c
extern volatile bool spi_setup_completed;

// From controller.c
extern analog_filter sensed_motor_position[CONTROLLER_MOTORS_PER_MODULE];

// Our metrics
u64 i2c_timer_count = 0;
u64 spi_timer_count = 0;
motor_power_data_t motor_power_data[I2C_DEVICE_PAC1954_SENSOR_COUNT];


/**
 * The current temperature of the board, in freedom degrees 🦅
 */
double board_temperature;

/**
 * This timer is called to read the i2c-based sensors and update our global state.
 *
 * Due to the nature of i2c, this one isn't called as often as the SPI sensor loop. It
 * reads the temperature and the amount of current our motors are drawing.
 */
TimerHandle_t i2c_sensor_read_timer = NULL;

/**
 * Same as the above, but for SPI.
 *
 * This one is much faster than the I2C timer. The main thing on it is the sensor to read
 * the position of the servos.
 */
TimerHandle_t spi_sensor_read_timer = NULL;


/**
 * Set up the sensors
 *
 * Note that this requires the i2c bus to be initialized first. (ie, call `setup_i2c()` first)
 */
void sensor_init() {

    debug("initializing sensors");

    // If these aren't true, very bad things will happen
    configASSERT(i2c_setup_completed);
    configASSERT(spi_setup_completed);

    // Set up the filter for the board temperature
    board_temperature = 66.6;

    // Initialize the motor power data metrics
    for(int i = 0; i < I2C_DEVICE_PAC1954_SENSOR_COUNT; i++) {
        motor_power_data[i].voltage = 0.0f;
        motor_power_data[i].current = 0.0f;
        motor_power_data[i].power = 0.0f;
    }



}

void sensor_start() {

    debug("starting sensors");

    // Create the timer that reads the i2c sensors
    i2c_sensor_read_timer = xTimerCreate(
            "I2C Sensor Read Timer",                    // Timer name
            pdMS_TO_TICKS(SENSOR_I2C_TIMER_TIME_MS),    // Fire every SENSOR_TIMER_TIME_MS
            pdTRUE,                                     // Auto-reload
            (void *) 0,                                 // Timer ID (not used here)
            i2c_sensor_read_timer_callback              // Callback function
    );

    // Make sure this gets created
    configASSERT(i2c_sensor_read_timer != NULL);

    // Start the timer
    xTimerStart(i2c_sensor_read_timer, SENSOR_I2C_TIMER_TIME_MS / 2);

    info("started i2c sensor read timer");



    // Create the timer that reads the spi sensors
    spi_sensor_read_timer = xTimerCreate(
            "SPI Sensor Read Timer",                    // Timer name
            pdMS_TO_TICKS(SENSOR_SPI_TIMER_TIME_MS),    // Fire every SENSOR_SPI_TIMER_TIME_MS
            pdTRUE,                                     // Auto-reload
            (void *) 0,                                 // Timer ID (not used here)
            spi_sensor_read_timer_callback              // Callback function
    );

    // Make sure this gets created
    configASSERT(spi_sensor_read_timer != NULL);

    // Start the timer, saying that it's okay to wait half the allowed time before starting
    xTimerStart(spi_sensor_read_timer, pdMS_TO_TICKS(SENSOR_SPI_TIMER_TIME_MS) / 2);

    info("started spi sensor read timer");
}

/*
 * Note to future me! 😅
 *
 * Remember that's there's only one I2C bus in use, and we can't have
 * several things trying to use it at once.
 *
 * As tempting as it might seem, do not break the timer callback functions
 * up into separate timers. It will not end well.
 */


void i2c_sensor_read_timer_callback(TimerHandle_t xTimer) {

    // We don't use this on this timer
    (void) xTimer;

    board_temperature = mcp9808_read_temperature_f(SENSORS_I2C_BUS, I2C_DEVICE_MCP9808);
    verbose("board temperature: %.2fF", board_temperature);

    for(int i = 0; i < I2C_DEVICE_PAC1954_SENSOR_COUNT; i++) {
        motor_power_data[i].voltage = pac1954_read_voltage(i);
        motor_power_data[i].current = pac1954_read_current(i);
        motor_power_data[i].power = pac1954_read_power(i);
    }
    verbose("motor power data: %f %f %f %f",
            motor_power_data[0].power,
            motor_power_data[1].power,
            motor_power_data[2].power,
            motor_power_data[3].power);

    // Send refresh command to get fresh data for next pass
    pac1954_refresh();
    i2c_timer_count += 1;

}



void spi_sensor_read_timer_callback(TimerHandle_t xTimer) {

    // We don't use this on this timer
    (void) xTimer;

    // Read all the pins and update the filter map
    for(int i = 0; i < CONTROLLER_MOTORS_PER_MODULE; i++) {

        // Read the ADC value
        u16 adc_value = adc_read(i, SENSORS_SPI_CS_PIN);

        // Update the filter
        analog_filter_update(&sensed_motor_position[i], adc_value);

    }

    spi_timer_count += 1;

    if(spi_timer_count % 500 == 0) {
        debug("sensed motor positions: %u %u %u %u",
              analog_filter_get_value(&sensed_motor_position[0]),
              analog_filter_get_value(&sensed_motor_position[1]),
              analog_filter_get_value(&sensed_motor_position[2]),
              analog_filter_get_value(&sensed_motor_position[3]));
    }


}
