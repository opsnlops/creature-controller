
#include "controller-config.h"

#include <memory>

#include <FreeRTOS.h>
#include <task.h>

// TinyUSB
#include "bsp/board.h"
#include "tusb.h"


#include "freertos_extra.h"

#include "pico/stdlib.h"

#include "io/usb_serial.h"
#include "logging/logging.h"
#include "usb/usb.h"

#include "debug/blinker.h"


volatile size_t xFreeHeapSpace;

int main() {

    // All the SDK to bring up the stdio stuff, so we can write to the serial port
    stdio_init_all();


    //printf("started");

    logger_init();
    debug("Logging running!");


    start_debug_blinker();

    board_init();
    start_usb_tasks();

    // Fire up the incoming serial reader
    usb_serial_init();
    start_incoming_usb_serial_reader();

    // And fire up the tasks!
    vTaskStartScheduler();
}
