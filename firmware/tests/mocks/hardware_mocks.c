/**
 * @file hardware_mocks.c
 * @brief Mock implementations of Pico SDK hardware functions
 */

#include "hardware_mocks.h"
#include <string.h>
#include <stdlib.h>

// --- GPIO Mocks ---

static u8 gpio_pin_states[40] = {0}; // Track state of GPIO pins
static gpio_irq_callback_t gpio_callbacks[40] = {NULL};

void gpio_init_mock(u32 gpio) {
    // Just record that init was called
}

void gpio_set_dir_mock(u32 gpio, bool out) {
    // Just record direction change
}

void gpio_put_mock(u32 gpio, bool value) {
    if (gpio < 40) {
        gpio_pin_states[gpio] = value ? 1 : 0;
    }
}

bool gpio_get_mock(u32 gpio) {
    if (gpio < 40) {
        return gpio_pin_states[gpio];
    }
    return false;
}

// --- I2C Mocks ---

static u8 i2c_mock_regs[128][256]; // Mock I2C registers for devices
static bool i2c_device_exists[128] = {false}; // Track which I2C addresses exist

void i2c_init_mock(i2c_inst_t *i2c, u32 baudrate) {
    // Just record that init was called
}

int i2c_write_blocking_mock(i2c_inst_t *i2c, u8 addr, const u8 *src, size_t len, bool nostop) {
    if (addr < 128 && i2c_device_exists[addr]) {
        // If first byte is register address, store data in that register
        if (len > 1) {
            u8 reg = src[0];
            for (size_t i = 1; i < len; i++) {
                i2c_mock_regs[addr][reg + (i-1)] = src[i];
            }
        }
        return len;
    }
    return -1; // Device doesn't exist
}

int i2c_read_blocking_mock(i2c_inst_t *i2c, u8 addr, u8 *dst, size_t len, bool nostop) {
    if (addr < 128 && i2c_device_exists[addr]) {
        // If register address was previously specified, read from that register
        // For simplicity, we'll use a global last_reg variable
        static u8 last_reg = 0;
        for (size_t i = 0; i < len; i++) {
            dst[i] = i2c_mock_regs[addr][last_reg + i];
        }
        return len;
    }
    return -1; // Device doesn't exist
}

// --- SPI Mocks ---

static u8 spi_tx_data[256]; // Store data written to SPI
static u8 spi_rx_data[256]; // Data to return on SPI read

void spi_init_mock(spi_inst_t *spi, u32 baudrate) {
    // Just record that init was called
    memset(spi_tx_data, 0, sizeof(spi_tx_data));
    memset(spi_rx_data, 0, sizeof(spi_rx_data));
}

int spi_write_blocking_mock(spi_inst_t *spi, const u8 *src, size_t len) {
    if (len > sizeof(spi_tx_data)) {
        len = sizeof(spi_tx_data);
    }
    memcpy(spi_tx_data, src, len);
    return len;
}

int spi_read_blocking_mock(spi_inst_t *spi, u8 repeated_tx_data, u8 *dst, size_t len) {
    if (len > sizeof(spi_rx_data)) {
        len = sizeof(spi_rx_data);
    }
    memcpy(dst, spi_rx_data, len);
    return len;
}

int spi_write_read_blocking_mock(spi_inst_t *spi, const u8 *src, u8 *dst, size_t len) {
    if (len > sizeof(spi_tx_data)) {
        len = sizeof(spi_tx_data);
    }
    memcpy(spi_tx_data, src, len);
    memcpy(dst, spi_rx_data, len);
    return len;
}

// --- Utility Functions ---

void reset_gpio_mocks(void) {
    memset(gpio_pin_states, 0, sizeof(gpio_pin_states));
    memset(gpio_callbacks, 0, sizeof(gpio_callbacks));
}

void reset_i2c_mocks(void) {
    memset(i2c_mock_regs, 0, sizeof(i2c_mock_regs));
    memset(i2c_device_exists, 0, sizeof(i2c_device_exists));
}

void reset_spi_mocks(void) {
    memset(spi_tx_data, 0, sizeof(spi_tx_data));
    memset(spi_rx_data, 0, sizeof(spi_rx_data));
}

void set_i2c_device_exists(u8 addr, bool exists) {
    if (addr < 128) {
        i2c_device_exists[addr] = exists;
    }
}

void set_i2c_register_value(u8 addr, u8 reg, u8 value) {
    if (addr < 128) {
        i2c_mock_regs[addr][reg] = value;
    }
}

void set_spi_rx_data(const u8 *data, size_t len) {
    if (len > sizeof(spi_rx_data)) {
        len = sizeof(spi_rx_data);
    }
    memcpy(spi_rx_data, data, len);
}

u8* get_spi_tx_data(void) {
    return spi_tx_data;
}

// --- Time Functions ---

absolute_time_t get_absolute_time_mock(void) {
    static u64 fake_time = 0;
    absolute_time_t t;
    t.t = fake_time;
    fake_time += 1000; // Increment by 1ms each call
    return t;
}

u32 to_ms_since_boot_mock(absolute_time_t t) {
    return t.t / 1000;
}