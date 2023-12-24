
#include "controller-config.h"

#include <memory>

#include <FreeRTOS.h>
#include <task.h>

// TinyUSB
#include "bsp/board.h"
#include "tusb.h"


#include "freertos_extra.h"

#include "pico/stdlib.h"

#include "controller/controller.h"
#include "debug/remote_logging.h"
#include "debug/stats_reporter.h"
#include "io/usb_serial.h"
#include "logging/logging.h"
#include "usb/usb.h"

#include "debug/blinker.h"


volatile size_t xFreeHeapSpace;

int main() {

    // All the SDK to bring up the stdio stuff, so we can write to the serial port
    stdio_init_all();

    logger_init();
    debug("Logging running!");

    //start_debug_blinker();

    board_init();
    creatures::usb::init();
    creatures::usb::start();


    // Fire up the stats reporter
    creatures::debug::start_stats_reporter();


    // Start the controller
    creatures::controller::init();
    creatures::controller::start();

//#warning "Remote log testing active!"
    start_debugging_remote_logging();

    // And fire up the tasks!
    vTaskStartScheduler();
}
