
#pragma once

#include "controller-config.h"

#include <FreeRTOS.h>
#include <task.h>


/*
 * This file is a crude sort of task manager, listing all the tasks
 * we have.
 */

portTASK_FUNCTION_PROTO(debug_console_task, pvParameters);              // used in debug/shell.cpp
portTASK_FUNCTION_PROTO(displayUpdateTask, pvParameters);               // used in display.cpp
portTASK_FUNCTION_PROTO(log_queue_reader_task, pvParameters);           // used in logging/logging.cpp
portTASK_FUNCTION_PROTO(dmx_processing_task, pvParameters);             // used in io/dmx.cpp
portTASK_FUNCTION_PROTO(creature_worker_task, pvParameters);            // used by the creature
portTASK_FUNCTION_PROTO(controller_housekeeper_task, pvParameters);     // used by the controller
portTASK_FUNCTION_PROTO(status_lights_task, pvParameters);              // used by the status lights


portTASK_FUNCTION_PROTO(controller_motor_setup_task, pvParameters);     // used by the controller
