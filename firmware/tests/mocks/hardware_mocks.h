/**
 * @file hardware_mocks.h
 * @brief Mock implementations of Pico SDK hardware functions
 */

#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include "types.h"

// --- Type Definitions ---

typedef struct {
    u64 t;
} absolute_time_t;

typedef void* i2c_inst_t;
typedef void* spi_inst_t;
typedef void* pio_t;
typedef u32 pio_sm_t;

typedef void (*gpio_irq_callback_t)(u32 gpio, u32 events);

// Define hardware constants
#define GPIO_OUT 1
#define GPIO_IN 0
#define GPIO_FUNC_SPI 1
#define GPIO_FUNC_I2C 2
#define GPIO_FUNC_PWM 3
#define GPIO_FUNC_UART 4

// --- GPIO Functions ---
void gpio_init_mock(u32 gpio);
void gpio_set_dir_mock(u32 gpio, bool out);
void gpio_put_mock(u32 gpio, bool value);
bool gpio_get_mock(u32 gpio);

// --- I2C Functions ---
void i2c_init_mock(i2c_inst_t *i2c, u32 baudrate);
int i2c_write_blocking_mock(i2c_inst_t *i2c, u8 addr, const u8 *src, size_t len, bool nostop);
int i2c_read_blocking_mock(i2c_inst_t *i2c, u8 addr, u8 *dst, size_t len, bool nostop);

// --- SPI Functions ---
void spi_init_mock(spi_inst_t *spi, u32 baudrate);
int spi_write_blocking_mock(spi_inst_t *spi, const u8 *src, size_t len);
int spi_read_blocking_mock(spi_inst_t *spi, u8 repeated_tx_data, u8 *dst, size_t len);
int spi_write_read_blocking_mock(spi_inst_t *spi, const u8 *src, u8 *dst, size_t len);

// --- Time Functions ---
absolute_time_t get_absolute_time_mock(void);
u32 to_ms_since_boot_mock(absolute_time_t t);

// --- Utility Functions ---
void reset_gpio_mocks(void);
void reset_i2c_mocks(void);
void reset_spi_mocks(void);
void set_i2c_device_exists(u8 addr, bool exists);
void set_i2c_register_value(u8 addr, u8 reg, u8 value);
void set_spi_rx_data(const u8 *data, size_t len);
u8* get_spi_tx_data(void);

// --- Macros to replace hardware functions with mocks ---
#define gpio_init gpio_init_mock
#define gpio_set_dir gpio_set_dir_mock
#define gpio_put gpio_put_mock
#define gpio_get gpio_get_mock

#define i2c_init i2c_init_mock
#define i2c_write_blocking i2c_write_blocking_mock
#define i2c_read_blocking i2c_read_blocking_mock

#define spi_init spi_init_mock
#define spi_write_blocking spi_write_blocking_mock
#define spi_read_blocking spi_read_blocking_mock
#define spi_write_read_blocking spi_write_read_blocking_mock

#define get_absolute_time get_absolute_time_mock
#define to_ms_since_boot to_ms_since_boot_mock

// Hardware instance definitions
extern i2c_inst_t *i2c0;
extern i2c_inst_t *i2c1;
extern spi_inst_t *spi0;
extern spi_inst_t *spi1;