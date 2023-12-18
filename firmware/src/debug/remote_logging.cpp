
#include <cstring>

#include <FreeRTOS.h>
#include <task.h>

#include "logging/logging.h"

#include "tasks.h"

extern TaskHandle_t debug_remote_logging_task_handle;


void start_debugging_remote_logging() {

    xTaskCreate(debug_remote_logging_task,
                "debug_remote_logging_task",
                256,
                nullptr,
                1,
                &debug_remote_logging_task_handle);
}

portTASK_FUNCTION(debug_remote_logging_task, pvParameters) {

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for (EVER) {

        while (true) {

            verbose("this is a verbose message");
            debug("this is a debug message");
            info("this is an info message");
            warning("this is a warning message");
            error("this is an error message");
            fatal("this is a fatal message");

            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
#pragma clang diagnostic pop
}
