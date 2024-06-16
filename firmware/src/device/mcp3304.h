
#pragma

#include "hardware/spi.h"

#include "controller-config.h"



/**
 * My own mcp3304 library
 *
 * @param adc_channel which ADC
 * @param adc_num_cs_pin the chip select pin
 * @return the reading from the ADC
 */
u16 adc_read(u8 adc_channel, u8 adc_num_cs_pin);



