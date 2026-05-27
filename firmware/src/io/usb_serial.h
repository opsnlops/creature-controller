
#pragma once

#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "types.h"

void usb_serial_init();
void usb_serial_start();

portTASK_FUNCTION_PROTO(incoming_usb_serial_reader_task, pvParameters);
