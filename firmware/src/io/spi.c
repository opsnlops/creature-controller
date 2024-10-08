#include "pico/stdlib.h"
#include "pico/stdio.h"

#include "logging/logging.h"
#include "io/spi.h"

#include "controller-config.h"
#include "types.h"

// This is a flag to indicate that the SPI bus has been set up
volatile bool spi_setup_completed = false;

/**
 * @brief Set up the SPI bus for the sensors
 *
 * Configure SPI as we usually do. This is the same config that's used on the Creature
 * Joystick, which I know works very well.
 *
 */
void setup_spi() {

    debug("setting up spi");

    spi_init(SENSORS_SPI_BUS, SENSORS_SPI_FREQ);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SENSORS_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SENSORS_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SENSORS_SPI_RX_PIN, GPIO_FUNC_SPI);

    // Set up chip select independently in case we have more than one ADC
    gpio_init(SENSORS_SPI_CS_PIN);
    gpio_set_dir(SENSORS_SPI_CS_PIN, GPIO_OUT);
    gpio_put(SENSORS_SPI_CS_PIN, 1);


    spi_setup_completed = true;
    info("set up spi on bus %u, freq %u", SENSORS_SPI_BUS == spi0 ? 0 : 1, SENSORS_SPI_FREQ);
}