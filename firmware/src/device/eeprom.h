
#pragma once


#include "hardware/i2c.h"

#include "types.h"

// The magic string at the top of our binary
#define MAGIC_WORD "HOP!"
#define MAGIC_WORD_SIZE 4

void eeprom_setup_i2c();
void eeprom_read(i2c_inst_t *i2c, u8 eeprom_addr, u16 mem_addr, u8 *data, size_t len);
void read_eeprom_and_configure();
int parse_eeprom_data(const u8 *data, size_t len);
int extract_string(const u8 *data, size_t len, size_t *offset,
                   char *buffer, size_t bufsize, const char *fieldName);

