
#include "pico/time.h"

#include "io/usb_serial.h"
#include "logging/logging.h"
#include "messaging/messaging.h"

#include "controller-config.h"

bool handlePingMessage(const GenericMessage *msg) {

    debug("handling ping message");

    // Look at the time
    debug("received ping with time: %s", msg->tokens[0]);

    // Send back a pong
    char message[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH];
    memset(&message, '\0', USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);

    snprintf(message, USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH,
             "PONG\t%lu", to_ms_since_boot(get_absolute_time()));

    send_to_controller(message);
    debug("sent back a pong");

    return true;

}

