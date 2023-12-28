
#pragma once

#include <FreeRTOS.h>
#include <timers.h>


void start_stats_reporter();
void statsReportTimerCallback(TimerHandle_t xTimer);
