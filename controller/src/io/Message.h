
#pragma once

#include "config/UARTDevice.h"

namespace creatures::io {

    using creatures::config::UARTDevice;

    /**
     * A message to be sent to a UART module
     */
    struct Message {
        // The module that the message is intended for
        UARTDevice::module_name module;

        // The payload of the message
        std::string payload;

        Message(UARTDevice::module_name mod, std::string pay)
                : module(mod), payload(std::move(pay)) {}
    };

} //creatures::io
