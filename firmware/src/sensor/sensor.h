
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
 * Callback for the i2c-based sensor read timer.
 *
 * @param xTimer The timer that called this function
 */
void i2c_sensor_read_timer_callback(TimerHandle_t xTimer);


#ifdef CC_VER2

/**
 * Callback for the spi-based sensor read timer.
 *
 * @param xTimer The timer that called this function
 */
void spi_sensor_read_timer_callback(TimerHandle_t xTimer);
#endif





