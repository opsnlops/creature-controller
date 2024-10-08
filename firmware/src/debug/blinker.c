
#include <stddef.h>

#include <FreeRTOS.h>
#include <task.h>

#include "pico/stdlib.h"
#include "pico/stdio.h"

#include "debug/blinker.h"
#include "logging/logging.h"

#include "types.h"

// From blinker.h
extern TaskHandle_t debug_blinker_task_handle;


void start_debug_blinker() {

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    xTaskCreate(debug_blinker_task,
                "debug_blinker_task",
                256,
                NULL,
                1,
                &debug_blinker_task_handle);
}

portTASK_FUNCTION(debug_blinker_task, pvParameters) {

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for (EVER) {

        bool isOn = true;
        u8 direction = 1;

        while (true) {

            if (isOn) {
                direction = 1;
            } else {
                direction = 0;
            }

            gpio_put(PICO_DEFAULT_LED_PIN, direction);
            isOn = !isOn;

             vTaskDelay(pdMS_TO_TICKS(250));
        }
    }
#pragma clang diagnostic pop
}
