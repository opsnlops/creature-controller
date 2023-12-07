#include <sys/select.h>
#include <sys/cdefs.h>

#ifndef CREATURES_USB_H_
#define CREATURES_USB_H_

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
void cdc_send(char* buf);

#ifdef __cplusplus
}
#endif

#endif /* CREATURES_USB_H_ */