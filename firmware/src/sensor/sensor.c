
#include <stddef.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "controller-config.h"

#include "controller/controller.h"
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



/**
 * This timer is called to read the i2c-based sensors and update our global state.
 *
 * Due to the nature of i2c, this one isn't called as often as the SPI sensor loop. It
 * reads the temperature and the amount of current our motors are drawing.
 */
TimerHandle_t i2c_sensor_read_timer = NULL;


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
    xTimerStart(i2c_sensor_read_timer, 50);

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


void i2c_sensor_read_timer_callback(TimerHandle_t xTimer) {

    // We don't use this on this timer
    (void) xTimer;


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


    if(spi_timer_count % 25 == 0) {
        debug("sensed motor positions: %u %u %u %u",
              analog_filter_get_value(&sensed_motor_position[0]),
              analog_filter_get_value(&sensed_motor_position[1]),
              analog_filter_get_value(&sensed_motor_position[2]),
              analog_filter_get_value(&sensed_motor_position[3]));
    }

}






u16 adc_read(uint8_t adc_channel, uint8_t adc_num_cs_pin) {

    // Command to read from a specific channel in single-ended mode
    // Start bit, SGL/DIFF, and D2 bit of the channel
    uint8_t cmd0 = 0b00000110 | ((adc_channel & 0b100) >> 2);
    uint8_t cmd1 = (adc_channel & 0b011) << 6; // Remaining channel bits positioned

    uint8_t txBuffer[3] = {cmd0, cmd1, 0x00}; // The last byte doesn't matter, it's just to clock out the ADC data
    uint8_t rxBuffer[3] = {0}; // To store the response

    gpio_put(adc_num_cs_pin, 0); // Activate CS to start the transaction
    spi_write_read_blocking(spi0, txBuffer, rxBuffer, 3); // Send the command and read the response
    gpio_put(adc_num_cs_pin, 1); // Deactivate CS to end the transaction

    // Now, interpret the response:
    // Skip the first 6 bits of rxBuffer[1], then take the next 10 bits as the ADC value
    uint16_t adcResult = ((rxBuffer[1] & 0x0F) << 8) | rxBuffer[2];

    // Debug print
    /*
    if (adc_channel == 0)
        debug("ADC Channel: %d, Raw SPI Data: %s %s %s, ADC Result: %u",
               adc_channel,
               to_binary_string(rxBuffer[0]),
               to_binary_string(rxBuffer[1]),
               to_binary_string(rxBuffer[2]),
               adcResult);
    */

    return adcResult;
}


const char* to_binary_string(u8 value) {
    static char bStr[9];
    bStr[8] = '\0'; // Null terminator
    for (int i = 7; i >= 0; i--) {
        bStr[7 - i] = (value & (1 << i)) ? '1' : '0';
    }
    return bStr;
}