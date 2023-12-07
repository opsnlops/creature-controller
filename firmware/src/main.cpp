
#include "controller-config.h"

#include <memory>

#include <FreeRTOS.h>
#include <task.h>

// TinyUSB
#include "bsp/board.h"
#include "tusb.h"

#include "pico/stdlib.h"

#include "logging/logging.h"

int main() {

    // All the SDK to bring up the stdio stuff, so we can write to the serial port
    stdio_init_all();

    logger_init();
    debug("Logging running!");


    board_init();
    //start_usb_tasks();

    // And fire up the tasks!
    vTaskStartScheduler();
}
