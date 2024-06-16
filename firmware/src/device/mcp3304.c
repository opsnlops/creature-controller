
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "logging/logging.h"
#include "util/string_utils.h"


#include "controller-config.h"
#include "mcp3304.h"

// From spi.c
extern volatile bool spi_setup_completed;


u16 adc_read(uint8_t adc_channel, uint8_t adc_num_cs_pin) {

    // This code will go boom if SPI isn't set up first
    configASSERT(spi_setup_completed);

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

