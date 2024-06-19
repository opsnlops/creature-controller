
#pragma once


#include <stdio.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "logging/logging.h"


#include "controller-config.h"

#define PAC1954_PRODUCT_ID_REGISTER         0xFD
#define PAC1954_MANUFACTURER_ID_REGISTER    0xFE
#define PAC1954_REVISION_ID_REGISTER        0xFF


void init_pac1954(i2c_inst_t *i2c, u8 address);


u8 pac1954_read_register_8bit(i2c_inst_t *i2c, u8 address, u8 register_address);
u16 pac1954_read_register_16bit(i2c_inst_t *i2c, u8 address, u8 register_address);
