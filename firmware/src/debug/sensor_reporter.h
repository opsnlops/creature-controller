
#pragma once

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include "controller/controller.h"
#include "debug/sensor_reporter.h"
#include "io/message_processor.h"

void start_sensor_reporter();
void sensorReportTimerCallback(TimerHandle_t xTimer);
