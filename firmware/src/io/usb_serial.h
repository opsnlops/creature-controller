
#pragma once

#include <string>

#include "controller-config.h"


void usb_serial_init();
void start_serial_tasks();
bool inline is_safe_to_enqueue_incoming_usb_serial();
bool inline is_safe_to_enqueue_outgoing_usb_serial();

bool send_to_controller(const char* message);
