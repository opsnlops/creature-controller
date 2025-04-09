#include "pico/stdlib.h"
#include "pico/stdio.h"

#include "logging/logging.h"
#include "io/spi.h"

#include "controller/config.h"
#include "types.h"

/**
 * @brief Flag indicating that the SPI bus has been set up
 */
volatile bool spi_setup_completed = false;

/**
 * @brief Set up the SPI bus for the sensors
 *
 * Configure SPI with standard settings for sensor communication:
 * - 8-bit data
 * - Clock polarity 0 (clock idles low)
 * - Clock phase 0 (sample on rising edge)
 * - MSB first transmission
 *
 * @return true if SPI was successfully initialized, false otherwise
 */
bool setup_spi(void) {
    debug("Setting up SPI...");

    // Initialize the SPI peripheral with the configured frequency
    spi_init(SENSORS_SPI_BUS, SENSORS_SPI_FREQ);

    // Configure SPI format
    spi_set_format(SENSORS_SPI_BUS,
                   8,                // 8 bits per transfer
                   SPI_CPOL_0,      // Clock polarity (idle low)
                   SPI_CPHA_0,      // Clock phase (sample on rising edge)
                   SPI_MSB_FIRST);  // Bit order

    // Note: spi_init and spi_set_format don't return status values in the Pico SDK,
    // so we can't check for errors here directly

    // Configure SPI pins
    gpio_set_function(SENSORS_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SENSORS_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SENSORS_SPI_RX_PIN, GPIO_FUNC_SPI);

    // Set up chip select pin as GPIO output
    gpio_init(SENSORS_SPI_CS_PIN);
    gpio_set_dir(SENSORS_SPI_CS_PIN, GPIO_OUT);
    gpio_put(SENSORS_SPI_CS_PIN, 1);  // Deselect device initially

    // Mark setup as complete
    spi_setup_completed = true;

    info("SPI initialized successfully (Bus: %u, Freq: %u Hz, CS: %u)",
         SENSORS_SPI_BUS == spi0 ? 0 : 1,
         SENSORS_SPI_FREQ,
         SENSORS_SPI_CS_PIN);

    return true;
}