

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/i2c.h"

#include "device/mcp9808.h"
#include "logging/logging.h"

#include "controller-config.h"
#include "types.h"

/*
 * A lot of this code comes from Microchip's Arduino library for the MCP9808. I've ported it
 * to the Pico, more or less.
 *
 * https://github.com/microchip-pic-avr-solutions/mcp9808_arduino_driver/blob/main/src/mcp9808.cpp
 *
 */

// From i2c.c
extern volatile bool i2c_setup_completed;

void init_mcp9808(i2c_inst_t *i2c, u8 address) {

    configASSERT(i2c_setup_completed);

    // Gather some device information
    u16 device_id = mcp9808_read_register(i2c, address, MCP9808_POINTER_DEVICE_ID);
    u16 manufacturer_id = mcp9808_read_register(i2c, address, MCP9808_POINTER_MANUF_ID);

    debug("MCP9808 device ID: 0x%02X", device_id);
    debug("MCP9808 manufacturer ID: 0x%02X", manufacturer_id);

    configASSERT(device_id == I2C_DEVICE_MCP9808_PRODUCT_ID);

    // Set the MCP9808 to continuous conversion mode
    mcp9808_write_register(i2c, address, MCP9808_POINTER_CONFIG, 0x0000);

    // Set the MCP9808 to 0.125C resolution
    mcp9808_set_resolution(i2c, address, res_0125);

    debug("MCP9808 initialized");

}


u16 mcp9808_read_register(i2c_inst_t *i2c, u8 address, u8 register_address) {

    configASSERT(i2c_setup_completed);

    uint8_t data[2];
    data[0] = register_address;
    i2c_write_blocking(i2c, address, data, 1, false);
    i2c_read_blocking(i2c, address, data, 2, false);
    return (data[0] << 8) | data[1];

}

void mcp9808_write_register(i2c_inst_t *i2c, u8 address, u8 register_address, u16 value) {

    configASSERT(i2c_setup_completed);

    u8 data[3];
    data[0] = register_address;
    data[1] = value >> 8;
    data[2] = value & 0xFF;
    i2c_write_blocking(i2c, address, data, 3, false);

}


double mcp9808_read_temperature_c(i2c_inst_t *i2c, u8 address) {

    configASSERT(i2c_setup_completed);

    u16 raw_temp = mcp9808_read_register(i2c, address, MCP9808_POINTER_AMBIENT_TEMP);

    u8 upper_byte = raw_temp >> 8;
    u8 lower_byte = (raw_temp & 0xFF);

    /* Clear flags */
    upper_byte = upper_byte & 0x1F;

    /* Check sign of temperature data */
    if ((upper_byte & 0x10) == 0x10) {  // T_A < 0
        upper_byte = upper_byte & 0x0F; // Clear sign
        return (256.0 - (upper_byte * 16.0 + lower_byte / 16.0)) * -1;
    } else { // T_A >= 0
        return (upper_byte * 16.0 + lower_byte / 16.0);
    }

}

double mcp9808_read_temperature_f(i2c_inst_t *i2c, u8 address) {
    double celsius = mcp9808_read_temperature_c(i2c, address);
    return ((celsius * 1.8) + 32.0);
}

u16 mcp9808_get_resolution(i2c_inst_t *i2c, u8 address) {

    configASSERT(i2c_setup_completed);

    u16 config = mcp9808_read_register(i2c, address, MCP9808_POINTER_CONFIG);

    /* Return resolution (factor x1e4) */
    switch (config) {
        case res_05:
            return 5000;
        case res_025:
            return 2500;
        case res_0125:
            return 1250;
        case res_00625:
            return 625;

        default:
            warning("Error: mcp9808 resolution not in list %u", config);
            return 1;
    }

}

void mcp9808_set_resolution(i2c_inst_t *i2c, u8 address, mcp9808_res_t resolution) {

    configASSERT(i2c_setup_completed);

    u16 config = mcp9808_read_register(i2c, address, MCP9808_POINTER_CONFIG);
    config = config & 0xFF9F; // Clear resolution bits
    config = config | (resolution << 5);
    mcp9808_write_register(i2c, address, MCP9808_POINTER_CONFIG, config);

    debug("Set mcp9808 resolution to %d", resolution);
}