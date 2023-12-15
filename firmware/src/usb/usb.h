
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
#include <task.h>

// TinyUSB
#include "tusb.h"

#define BOARD_TUD_RHPORT     0

// Increase stack size when debug log is enabled
#define USBD_STACK_SIZE    (3*configMINIMAL_STACK_SIZE/2) * (CFG_TUSB_DEBUG ? 2 : 1)
#define HID_STACK_SIZE      configMINIMAL_STACK_SIZE

_Noreturn
portTASK_FUNCTION_PROTO(usb_device_task, pvParameters);

void start_usb_tasks();


#ifdef __cplusplus
}
#endif

// Back to C++!
void cdc_send(std::string line);

