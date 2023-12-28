

#include "controller-config.h"

#include <FreeRTOS.h>
#include <task.h>


volatile TaskHandle_t debug_blinker_task_handle;
volatile TaskHandle_t log_queue_reader_task_handle;
volatile TaskHandle_t status_lights_task_handle;
