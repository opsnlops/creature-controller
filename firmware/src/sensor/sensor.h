
#pragma once

#include <FreeRTOS.h>
#include <timers.h>

/*
 * Our Sensors!
 *
 * This file defines things about our sensor loop.
 */

void sensor_init();
void sensor_start();

/**
 * Callback for the sensor read timer.
 *
 * @param xTimer The timer that called this function
 */
void sensor_read_timer_callback(TimerHandle_t xTimer);
