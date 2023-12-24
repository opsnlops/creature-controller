
#pragma once

#include <string>

#include <FreeRTOS.h>
#include <task.h>

#include "controller-config.h"


namespace creatures::io::usb_serial {

    void init();
    void start();
    bool inline is_safe_to_enqueue_incoming_usb_serial();
    bool inline is_safe_to_enqueue_outgoing_usb_serial();

    bool send_to_controller(const char* message);

    portTASK_FUNCTION_PROTO(incoming_serial_reader_task, pvParameters);
    portTASK_FUNCTION_PROTO(outgoing_serial_writer_task, pvParameters);
}