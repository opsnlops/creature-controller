

#include "controller-config.h"

#include <FreeRTOS.h>
#include <task.h>


volatile TaskHandle_t debug_blinker_task_handle;
volatile TaskHandle_t debug_remote_logging_task_handle;
volatile TaskHandle_t stats_reporter_task_handle;

volatile TaskHandle_t log_queue_reader_task_handle;
volatile TaskHandle_t incoming_serial_reader_task_handle;
volatile TaskHandle_t outgoing_serial_writer_task_handle;



volatile TaskHandle_t controllerHousekeeperTaskHandle;
volatile TaskHandle_t controller_motor_setup_task_handle;
volatile TaskHandle_t status_lights_task_handle;
