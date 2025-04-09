#pragma once

#include "hardware/spi.h"
#include "controller/config.h"

/**
 * @brief Flag indicating if SPI setup has been completed successfully
 */
extern volatile bool spi_setup_completed;

/**
 * @brief Set up the SPI peripherals for sensor communication
 * 
 * Configures the SPI hardware with the pins and frequency defined in config.h.
 * Sets up the SPI bus with the correct polarity, phase, and bit order for
 * communicating with sensors. Also configures the chip select pin.
 * 
 * @return true if SPI was successfully initialized, false otherwise
 */
bool setup_spi(void);

/**
 * @brief Assert (activate) the SPI chip select line
 * 
 * Pulls the CS pin low to select the SPI device.
 */
static inline void spi_cs_select(void) {
    gpio_put(SENSORS_SPI_CS_PIN, 0);
}

/**
 * @brief Deassert (deactivate) the SPI chip select line
 * 
 * Pulls the CS pin high to deselect the SPI device.
 */
static inline void spi_cs_deselect(void) {
    gpio_put(SENSORS_SPI_CS_PIN, 1);
}