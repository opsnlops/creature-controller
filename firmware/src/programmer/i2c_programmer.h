
#pragma once

#include "hardware/i2c.h"



#define PROGRAMMER_SDA_PIN 2
#define PROGRAMMER_SCL_PIN 3
#define PROGRAMMER_I2C_BUS i2c1

#define PROGRAMMER_I2C_ADDR 0x50

portTASK_FUNCTION_PROTO(i2c_programmer_task, pvParameters);

void start_programmer_task();
void programmer_setup_i2c();
void programmer_program_i2c();


void i2c_eeprom_write(i2c_inst_t *i2c, uint8_t eeprom_addr, uint16_t mem_addr, const char *data, size_t len);
void print_eeprom_contents(const uint8_t *data, size_t len);
void i2c_eeprom_read(i2c_inst_t *i2c, uint8_t eeprom_addr, uint16_t mem_addr, uint8_t *data, size_t len);
bool verify_eeprom_data(i2c_inst_t *i2c, uint8_t eeprom_addr, const char *expected_data, size_t len);
