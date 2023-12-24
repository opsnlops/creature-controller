
#include "controller-config.h"

#include <memory>

#include <FreeRTOS.h>
#include <task.h>

// TinyUSB
#include "bsp/board.h"
#include "tusb.h"


#include "freertos_hooks.h"

#include "pico/stdlib.h"

#include "controller/controller.h"
#include "debug/remote_logging.h"
#include "debug/stats_reporter.h"
#include "io/usb_serial.h"
#include "logging/logging.h"
#include "usb/usb.h"

#include "tasks.h"

#include "debug/blinker.h"





volatile size_t xFreeHeapSpace;

int main() {

    // All the SDK to bring up the stdio stuff, so we can write to the serial port
    stdio_init_all();

    logger_init();
    debug("Logging running!");

    //start_debug_blinker();

    board_init();

    // Fire up the serial incoming and outgoing queues
    creatures::io::usb_serial::init();
    creatures::io::usb_serial::start();

    // Start the controller
    creatures::controller::init();
    creatures::controller::start();

    // Fire up the stats reporter
    creatures::debug::start_stats_reporter();

    // Queue up the startup task for right after the scheduler starts
    TaskHandle_t startup_task_handle;
    xTaskCreate(startup_task,
                "startup_task",
                configMINIMAL_STACK_SIZE,
                nullptr,
                1,
                &startup_task_handle);

    // And fire up the tasks!
    vTaskStartScheduler();
}


portTASK_FUNCTION(startup_task, pvParameters) {

    // These are in a task because the docs say:

    /*
        init device stack on configured roothub port
        This should be called after scheduler/kernel is started.
        Otherwise, it could cause kernel issue since USB IRQ handler does use RTOS queue API.
     */

    creatures::usb::init();
    creatures::usb::start();

   // Bye!
   vTaskDelete(nullptr);

}