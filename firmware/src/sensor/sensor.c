
#include <stddef.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "controller-config.h"

#include "logging/logging.h"

#include "ads1115.h"

#include "sensor.h"


// From i2c.c
extern volatile bool i2c_setup_completed;

struct ads1115_adc adc0;


/**
 * This timer is called to read the sensors and update our global state.
 *
 * This is a low priority timer, but it runs fairly often. It can be skipped every now
 * and then, but it's critical that it runs often enough to keep the sensors up to date.
 *
 * This timer is started in `sensor_start()`.
 *
 * As a first pass, I'm setting timer to run at double the rate of the servo loop.
 */
TimerHandle_t sensor_read_timer = NULL;



/**
 * Set up the sensors
 *
 * Note that this requires the i2c bus to be initialized first. (ie, call `setup_i2c()` first)
 */
void sensor_init() {

    debug("initializing sensors");

    // If this isn't true, very bad things will happen
    configASSERT(i2c_setup_completed);

    // Initialize the I2C bus
    ads1115_init(SENSORS_I2C_BUS, ADS1115_I2C_ADDR, &adc0);

    /*
     * Configure the ADC. We use single ended, 4.096V, 475 SPS.
     */
    ads1115_set_input_mux(ADS1115_MUX_SINGLE_0, &adc0);
    ads1115_set_pga(ADS1115_PGA_4_096, &adc0);
    ads1115_set_data_rate(ADS1115_RATE_475_SPS, &adc0);

    // Write the configuration for this to have an effect.
    ads1115_write_config(&adc0);

    info("configured adc0 on address 0x%02x", adc0.i2c_addr);

}

void sensor_start() {

    debug("starting sensors");

    // Create the timer that reads the sensors
    sensor_read_timer = xTimerCreate(
            "Sensor Read Timer",                    // Timer name
            pdMS_TO_TICKS(SENSOR_TIMER_TIME_MS),    // Fire every SENSOR_TIMER_TIME_MS
            pdTRUE,                                 // Auto-reload
            (void *) 0,                             // Timer ID (not used here)
            sensor_read_timer_callback              // Callback function
    );

    // Make sure this gets created
    configASSERT(sensor_read_timer != NULL);

    // Start the timer
    xTimerStart(sensor_read_timer, 5);

    info("started sensor read timer");

}

// State for the timer callback, so we're not allocating memory in each tick
u16 adc0_value;
u16 adc1_value;
u16 adc2_value;
u16 adc3_value;

volatile u64 sensor_read_count = 0;

void sensor_read_timer_callback(TimerHandle_t xTimer) {

    // We don't use this on this timer
    (void) xTimer;

    ads1115_set_input_mux(ADS1115_MUX_SINGLE_0, &adc0);
    ads1115_read_adc(&adc0_value, &adc0);

    ads1115_set_input_mux(ADS1115_MUX_SINGLE_1, &adc0);
    ads1115_read_adc(&adc1_value, &adc0);

    ads1115_set_input_mux(ADS1115_MUX_SINGLE_2, &adc0);
    ads1115_read_adc(&adc2_value, &adc0);

    ads1115_set_input_mux(ADS1115_MUX_SINGLE_3, &adc0);
    ads1115_read_adc(&adc3_value, &adc0);

    sensor_read_count = sensor_read_count + 1;

    if(sensor_read_count % 50 == 0) {
        debug("0: %u, 1: %u, 2: %u, 3: %u, count: %lld", adc0_value, adc1_value, adc2_value, adc3_value, sensor_read_count);
    }
}