
#include "controller-config.h"

#include <memory>

#include <FreeRTOS.h>
#include <task.h>

// TinyUSB
#include "bsp/board.h"
#include "tusb.h"

#include "pico/stdlib.h"

#include "logging/logging.h"
#include "usb/usb.h"

int main() {

    // All the SDK to bring up the stdio stuff, so we can write to the serial port
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, 1);
    gpio_put(PICO_DEFAULT_LED_PIN, !PICO_DEFAULT_LED_PIN_INVERTED);

    logger_init();
    debug("Logging running!");


    board_init();
    start_usb_tasks();

    // And fire up the tasks!
    vTaskStartScheduler();
}
