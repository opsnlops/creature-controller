
#include <stddef.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "logging/logging.h"
#include "debug/remote_logging.h"

#include "tasks.h"

TaskHandle_t debug_remote_logging_task_handle;



void start_debugging_remote_logging() {

    xTaskCreate(debug_remote_logging_task,
                "debug_remote_logging_task",
                256,
                NULL,
                1,
                &debug_remote_logging_task_handle);
}

portTASK_FUNCTION(debug_remote_logging_task, pvParameters) {

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for (EVER) {

        while (true) {

            verbose("this is a verbose messaging");
            debug("this is a debug messaging");
            info("this is an info messaging");
            warning("this is a warning messaging");
            error("this is an error messaging");
            fatal("this is a fatal messaging");

            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
#pragma clang diagnostic pop
}
