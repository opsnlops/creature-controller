
#pragma once

#include "controller-config.h"

#include <FreeRTOS.h>
#include <task.h>


/*
 * This file is a crude sort of task manager, listing all the tasks
 * we have.
 */

portTASK_FUNCTION_PROTO(debug_blinker_task, pvParameters);
portTASK_FUNCTION_PROTO(debug_remote_logging_task, pvParameters);

portTASK_FUNCTION_PROTO(log_queue_reader_task, pvParameters);           // used in logging/logging.cpp
portTASK_FUNCTION_PROTO(status_lights_task, pvParameters);              // used by the status lights


portTASK_FUNCTION_PROTO(controller_motor_setup_task, pvParameters);     // used by the controller


// Used by main.cpp to fire off things that should happen only AFTER
// the scheduler is running
portTASK_FUNCTION_PROTO(startup_task, pvParameters);
