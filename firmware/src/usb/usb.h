
#pragma once

#include <sys/select.h>
#include <sys/cdefs.h>
#include <string.h>


// FreeRTOS
#include <FreeRTOS.h>
#include <timers.h>

// TinyUSB
#include "tusb.h"

#include "types.h"

#define BOARD_TUD_RHPORT     0

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)
#define HID_STACK_SIZE      configMINIMAL_STACK_SIZE


void usb_init();

void usb_start();

/**
 * Called every 1ms to trigger the USB device timer
 * @param xTimer
 */
void usbDeviceTimerCallback(TimerHandle_t xTimer);

/**
 * @brief Timer to check and see if the CDC is connected
 *
 * Called every 100ms via a timer. If there's a change in state it will
 * both update the internal LED on the Pico, and call the hooks in
 * controller.c to react as needed.
 *
 * @param xTimer
 */
void is_cdc_connected_timer(TimerHandle_t xTimer);


void cdc_send(char const *line);
