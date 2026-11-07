
#pragma once

#include <string>

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

    Message(UARTDevice::module_name mod, std::string pay) : module(mod), payload(std::move(pay)) {}
};

/**
 * Can this message be discarded if the serial link falls behind?
 *
 * Position frames are replaced by the next frame and a lost ping is harmless.
 * Everything else, such as configuration and ESTOP, must always be delivered.
 */
inline bool isDroppableMessage(const Message &message) {
    return message.payload.starts_with("POS\t") || message.payload.starts_with("PING\t");
}

} // namespace creatures::io
