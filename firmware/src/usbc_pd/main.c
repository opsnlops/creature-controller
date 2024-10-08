#include <stddef.h>
#include <stdlib.h>


#include <FreeRTOS.h>
#include <task.h>

// TinyUSB
#include "bsp/board.h"
#include "tusb.h"

#include "pico/binary_info.h"
#include "pico/stdlib.h"

#include "device/status_lights.h"

#include "logging/logging.h"
#include "logging/logging_api.h"


#include "debug/blinker.h"

#include "version.h"

#include "usbc_pd/sensortest.h"

volatile size_t xFreeHeapSpace;

void acw_post_logging_hook(char *message, uint8_t message_length) {

    // Send the message to the console
    printf("%s\n", message);

}


int main() {

    bi_decl(bi_program_name("usbc_pd"))
    bi_decl(bi_program_description("April's Creature Workshop USB-C PD Controller Test"))
    bi_decl(bi_program_version_string(CREATURE_FIRMWARE_VERSION_STRING))
    bi_decl(bi_program_feature("FreeRTOS Version: " tskKERNEL_VERSION_NUMBER))
    bi_decl(bi_1pin_with_name(STATUS_LIGHTS_LOGIC_BOARD_PIN, "Status Lights for Logic Board"))


    // All the SDK to bring up the stdio stuff, so we can write to the serial port
    stdio_init_all();

    logger_init();
    debug("Logging running!");

    // Set up the board
    board_init();

    // Turn on the status lights
    status_lights_init();
    status_lights_start();

    // Set up our sensors
    usbpd_sensor_init();
    usbpd_sensor_start();

    start_debug_blinker();

    vTaskStartScheduler();
}