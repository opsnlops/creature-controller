
#pragma once

#include "controller-config.h"


void usb_serial_init();
void start_incoming_usb_serial_reader();
bool inline is_safe_to_enqueue_usb_serial();

