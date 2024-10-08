#include <stddef.h>
#include <stdlib.h>


#include <FreeRTOS.h>
#include <task.h>

#include "pico/binary_info.h"
#include "pico/stdlib.h"


#include "logging/logging.h"
#include "logging/logging_api.h"

#include "debug/blinker.h"

#include "programmer/i2c_programmer.h"

#include "tasks.h"
#include "version.h"

#include "usbc_pd/sensortest.h"

volatile size_t xFreeHeapSpace;

/** Print the message to the console */
void acw_post_logging_hook(char *message, uint8_t message_length) {

    // Send the message to the console
    printf("%s\n", message);

}


int main() {

    bi_decl(bi_program_name("programmer"))
    bi_decl(bi_program_description("April's Creature Workshop USB-C PD I2C EEPROM Programmer"))
    bi_decl(bi_program_version_string(CREATURE_FIRMWARE_VERSION_STRING))
    bi_decl(bi_program_feature("FreeRTOS Version: " tskKERNEL_VERSION_NUMBER))


    // All the SDK to bring up the stdio stuff, so we can write to the serial port
    stdio_init_all();

    logger_init();
    debug("Logging running!");

    programmer_setup_i2c();

    // Blinky the onboard LED so we know the cores are running
    start_debug_blinker();

    // Start the programmer task
    start_programmer_task();

    vTaskStartScheduler();
}