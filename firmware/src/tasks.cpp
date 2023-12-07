

#include "controller-config.h"

#include <FreeRTOS.h>
#include <task.h>


volatile TaskHandle_t debug_console_task_handle;
volatile TaskHandle_t displayUpdateTaskHandle;
volatile TaskHandle_t log_queue_reader_task_handle;
volatile TaskHandle_t dmx_processing_task_handle;
volatile TaskHandle_t controllerHousekeeperTaskHandle;
volatile TaskHandle_t controller_motor_setup_task_handle;
volatile TaskHandle_t status_lights_task_handle;
