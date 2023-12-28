
#pragma once

#include <sys/select.h>
#include <sys/cdefs.h>
#include <string.h>


// FreeRTOS
#include <FreeRTOS.h>
#include <timers.h>

// TinyUSB
#include "tusb.h"

#define BOARD_TUD_RHPORT     0

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)
#define HID_STACK_SIZE      configMINIMAL_STACK_SIZE





    void usb_init();
    void usb_start();
    void usbDeviceTimerCallback(TimerHandle_t xTimer);




void cdc_send(char const* line);
