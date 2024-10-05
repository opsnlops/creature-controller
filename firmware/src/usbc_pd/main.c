#include <stddef.h>
#include <stdlib.h>


#include <FreeRTOS.h>
#include <task.h>

// TinyUSB
#include "bsp/board.h"
#include "tusb.h"


#include "freertos_hooks.h"

#include "pico/binary_info.h"
#include "pico/stdlib.h"

#include "controller/controller.h"
#include "device/power_relay.h"
#include "device/status_lights.h"
#include "debug/remote_logging.h"
#include "debug/sensor_reporter.h"
#include "debug/stats_reporter.h"
#include "io/i2c.h"
#include "io/message_processor.h"
#include "io/spi.h"
#include "io/usb_serial.h"
#include "io/uart_serial.h"
#include "logging/logging.h"
#include "sensor/sensor.h"
#include "usb/usb.h"

#include "debug/blinker.h"

#include "tasks.h"
#include "version.h"

volatile size_t xFreeHeapSpace;

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

    start_debug_blinker();

    vTaskStartScheduler();
}