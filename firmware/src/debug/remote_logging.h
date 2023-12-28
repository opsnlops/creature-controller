
#pragma once

#include <FreeRTOS.h>
#include <task.h>

#include "tasks.h"

void start_debugging_remote_logging();
portTASK_FUNCTION_PROTO(debug_remote_logging_task, pvParameters);
