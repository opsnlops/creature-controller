/**
 * @file hardware_mocks_globals.c
 * @brief Global instances for hardware peripheral mocks
 */

#include "hardware_mocks.h"

// Define global hardware peripheral instances
i2c_inst_t i2c0_instance;
i2c_inst_t i2c1_instance;
spi_inst_t spi0_instance;
spi_inst_t spi1_instance;

// Pointers to the instances that can be used in tests
i2c_inst_t *i2c0 = &i2c0_instance;
i2c_inst_t *i2c1 = &i2c1_instance;
spi_inst_t *spi0 = &spi0_instance;
spi_inst_t *spi1 = &spi1_instance;