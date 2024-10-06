
#pragma once

#include <FreeRTOS.h>
#include <timers.h>



#define I2C_USBCPD_PAC1954                      0x10
#define I2C_USBCPD_PAC1954_SENSOR_COUNT         4

void usbpd_sensor_init();
void usbpd_sensor_start();

/**
 * Callback for the i2c-based sensor read timer.
 *
 * @param xTimer The timer that called this function
 */
void usbpd_i2c_sensor_read_timer_callback(TimerHandle_t xTimer);
