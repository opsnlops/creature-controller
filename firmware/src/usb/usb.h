
#pragma once

#include <sys/select.h>
#include <sys/cdefs.h>
#include <string>

// Mark this as being in C to C++ apps
#ifdef __cplusplus
extern "C"
{
#endif

// FreeRTOS
#include <FreeRTOS.h>
#include <timers.h>

// TinyUSB
#include "tusb.h"

#define BOARD_TUD_RHPORT     0

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)
#define HID_STACK_SIZE      configMINIMAL_STACK_SIZE

namespace creatures::usb {
    void init();
    void start();
    void usb_device_timer(TimerHandle_t xTimer);
}

#ifdef __cplusplus
}
#endif

// Back to C++!
void cdc_send(char const* line);
