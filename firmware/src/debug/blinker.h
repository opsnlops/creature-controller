
#pragma once

#include <FreeRTOS.h>
#include <task.h>

/**
 * This is a minimal code loop to blink an LED in a task, which is
 * a way to debug things when the serial output stops.
 */

portTASK_FUNCTION_PROTO(debug_blinker_task, pvParameters);

void start_debug_blinker();

