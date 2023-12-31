
#include <stddef.h>

#include "controller-config.h"

#include <FreeRTOS.h>
#include <task.h>

// TinyUSB
#include "bsp/board.h"
#include "tusb.h"


#include "freertos_hooks.h"

#include "pico/stdlib.h"

#include "controller/controller.h"
#include "device/power_relay.h"
#include "device/status_lights.h"
#include "debug/remote_logging.h"
#include "debug/stats_reporter.h"
#include "io/usb_serial.h"
#include "logging/logging.h"
#include "usb/usb.h"

#include "tasks.h"


volatile size_t xFreeHeapSpace;

int main() {

    // All the SDK to bring up the stdio stuff, so we can write to the serial port
    stdio_init_all();

    logger_init();
    debug("Logging running!");

    // Set up the board
    board_init();

    // Set up the power relay
    init_power_relay();

    // Fire up the serial incoming and outgoing queues
    usb_serial_init();
    usb_serial_start();

    // Start the controller
    controller_init();
    controller_start();

    // Fire up the stats reporter
    start_stats_reporter();

    // Turn on the status lights
    status_lights_init();
    status_lights_start();

    // Queue up the startup task for right after the scheduler starts
    TaskHandle_t startup_task_handle;
    xTaskCreate(startup_task,
                "startup_task",
                configMINIMAL_STACK_SIZE,
                NULL,
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

    usb_init();
    usb_start();

    // Bye!
    vTaskDelete(NULL);

}