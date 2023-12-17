
#pragma once

#include <FreeRTOS.h>
#include <task.h>

#include "tasks.h"

/**
 * This is a minimal code loop to blink an LED in a task, which is
 * a way to debug things when the serial output stops.
 */


void start_debug_blinker();

