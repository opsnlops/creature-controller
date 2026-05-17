
#include <stddef.h>

#include <FreeRTOS.h>
#include <queue.h>

#include "controller/config.h"

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "io/usb_serial.h"
#include "logging/logging.h"
#include "usb/usb.h"

#include "types.h"

extern volatile u64 usb_serial_messages_received;

// From message_processor.c - pipeline never blocks, it drops and counts.
extern volatile u64 incoming_messages_dropped;

// Our queue (inbound only; the outbound side is owned by usb_device_task)
extern QueueHandle_t usb_serial_incoming_commands;

// The global incoming messages queue
extern QueueHandle_t incoming_messages;

portTASK_FUNCTION(incoming_usb_serial_reader_task, pvParameters) {

    debug("hello from the serial reader!");

    // Bad things are gonna happen if this is null
    configASSERT(usb_serial_incoming_commands != NULL);

    // gpio_init(CDC_ACTIVE_PIN);
    // gpio_set_dir(CDC_ACTIVE_PIN, GPIO_OUT);
    bool isOn = true;
    uint8_t direction = 1;
    uint64_t count = 0L;

    // Define our buffer and zero it out
    char *rx_buffer = (char *)pvPortMalloc(sizeof(char) * USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);
    memset(rx_buffer, '\0', USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for (EVER) {

        if (xQueueReceive(usb_serial_incoming_commands, rx_buffer, (TickType_t)portMAX_DELAY) == pdPASS) {

            usb_serial_messages_received = usb_serial_messages_received + 1;

            // Create a buffer to hold the message and null it out
            char message[USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH];
            memset(message, '\0', USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);

            // Copy the message into the buffer
            strncpy(message, rx_buffer, USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);

            // Send this off to the incoming messages queue. Never block here:
            // a wedged consumer must not back up and stall the USB reader (and
            // through it, tud_task). Drop and count if the queue is full.
            if (xQueueSendToBack(incoming_messages, &message, 0) != pdTRUE)
                incoming_messages_dropped++;

            // Wipe out the buffer for next time
            memset(rx_buffer, '\0', USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);

            // Make the LED on the board blink
            if (isOn) {
                direction = 1;
            } else {
                direction = 0;
            }
            // gpio_put(CDC_ACTIVE_PIN, direction);
            isOn = !isOn;
        }
    }
#pragma clang diagnostic pop
}

// The outbound writer task lived here. It now lives in usb_device_task
// (src/usb/usb.c): TinyUSB is OPT_OS_NONE, so tud_task() and all CDC writes
// must run in one context. The USB task drains usb_serial_outgoing_messages
// directly.
