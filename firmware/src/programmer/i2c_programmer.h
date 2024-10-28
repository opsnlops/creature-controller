
#pragma once

#include "hardware/i2c.h"

#include "types.h"

#define PROGRAMMER_SDA_PIN 2
#define PROGRAMMER_SCL_PIN 3
#define PROGRAMMER_I2C_BUS i2c1

#define PROGRAMMER_I2C_ADDR 0x50


void programmer_setup_i2c();
void programmer_program_i2c();


void i2c_eeprom_write(i2c_inst_t *i2c, uint8_t eeprom_addr, uint16_t mem_addr, const u8 *data, size_t len);
void print_eeprom_contents(const u8 *data, size_t len);
void i2c_eeprom_read(i2c_inst_t *i2c, uint8_t eeprom_addr, uint16_t mem_addr, u8 *data, size_t len);
bool verify_eeprom_data(i2c_inst_t *i2c, uint8_t eeprom_addr, const u8 *expected_data, size_t len, char* result, size_t result_len);
void write_incoming_buffer_to_eeprom();