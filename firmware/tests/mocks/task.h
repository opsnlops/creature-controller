/**
 * @file task.h
 * @brief Mock FreeRTOS task header for test framework
 */

#pragma once

#include "freertos_mocks.h"

// Task macro definitions
#define portTASK_FUNCTION_PROTO(name, param) void name(void *param)
#define portTASK_FUNCTION(name, param) void name(void *param)
