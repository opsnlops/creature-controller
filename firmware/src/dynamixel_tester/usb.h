
#pragma once

#include <string.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "tusb.h"

#include "types.h"

#define BOARD_TUD_RHPORT 0

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE (3 * configMINIMAL_STACK_SIZE / 2) * (CFG_TUSB_DEBUG ? 2 : 1)

void usb_init();
void usb_start();

void usbDeviceTimerCallback(TimerHandle_t xTimer);
void is_cdc_connected_timer(TimerHandle_t xTimer);
void clear_data_transmission_lights_timer(TimerHandle_t xTimer);

void cdc_send(char const *line);
