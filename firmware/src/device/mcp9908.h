
#include <stdio.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "logging/logging.h"


#include "controller-config.h"


// Reference: https://github.com/microchip-pic-avr-solutions/mcp9808_arduino_driver/blob/main/src/mcp9808.cpp
#define MCP9808_POINTER_CONFIG          0x01 // MCP9808 configuration register
#define MCP9808_POINTER_UPPER_TEMP      0x02 // Upper alert boundary
#define MCP9808_POINTER_LOWER_TEMP      0x03 // Lower alert boundary
#define MCP9808_POINTER_CRIT_TEMP       0x04 // Critical temperature
#define MCP9808_POINTER_AMBIENT_TEMP    0x05 // Ambient temperature
#define MCP9808_POINTER_MANUF_ID        0x06 // Manufacture ID
#define MCP9808_POINTER_DEVICE_ID       0x07 // Device ID
#define MCP9808_POINTER_RESOLUTION      0x08 // Sensor resolution


typedef enum {
    res_05 = 0x00,
    res_025 = 0x01,
    res_0125 = 0x02,
    res_00625 = 0x03
} mcp9808_res_t;

/**
 * Initialize the MCP9908
 *
 * @param i2c the i2c instance
 * @param address address of the MCP9908
 */
void init_mcp9908(i2c_inst_t *i2c, u8 address);


/**
 * Read a register from the MCP9908
 *
 * @param i2c the i2c instance
 * @param address the address of the MCP9908
 * @param register_address the register to read
 * @return the value of the register
 */
u16 mcp9908_read_register(i2c_inst_t *i2c, u8 address, u8 register_address);

/**
 * Write a register to the MCP9908
 *
 * @param i2c the i2c instance
 * @param address the address of the MCP9908
 * @param register_address the register to write
 * @param value the value to write
 */
void mcp9908_write_register(i2c_inst_t *i2c, u8 address, u8 register_address, u16 value);


/**
 * Set the resolution of the MCP9908
 *
 * @param i2c the i2c device
 * @param address the i2c address of the MCP9908
 * @param resolution a value from the mcp9808_res_t enum
 */
void mcp9808_set_resolution(i2c_inst_t *i2c, u8 address, mcp9808_res_t resolution);


/**
* Read the temperature from the MCP9908
*
* @param i2c the i2c instance
* @param address the address of the MCP9908
* @return the temperature in degrees celsius
*/
u16 mcp9808_get_resolution(i2c_inst_t *i2c, u8 address);

/**
 * Read the temperature from the MCP9908
 *
 * @param i2c the i2c instance
 * @param address the address of the MCP9908
 * @return the temperature in degrees celsius
 */
double mcp9908_read_temperature_c(i2c_inst_t *i2c, u8 address);

/**
 * Read the temperature from the MCP9908
 *
 * @param i2c the i2c instance
 * @param address the address of the MCP9908
 * @return the temperature in freedom units
 */
double mcp9908_read_temperature_f(i2c_inst_t *i2c, u8 address);
