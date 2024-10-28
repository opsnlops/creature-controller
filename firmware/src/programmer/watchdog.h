
#pragma once

#include <hardware/watchdog.h>

#include <FreeRTOS.h>
#include <timers.h>


void start_watchdog_timer();
void watchdog_timer_callback(TimerHandle_t xTimer);
void reboot();
