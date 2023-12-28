
#include <stddef.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "controller-config.h"

#include "pico/stdlib.h"
#include "pico/stdio.h"

#include "logging/logging.h"
#include "io/usb_serial.h"
#include "messaging/messaging.h"

#include "usb/usb.h"


volatile u64 serial_messages_received = 0L;
volatile u64 serial_messages_sent = 0L;

extern QueueHandle_t usb_serial_incoming_commands;
extern QueueHandle_t usb_serial_outgoing_messages;

portTASK_FUNCTION(incoming_serial_reader_task, pvParameters) {

    debug("hello from the serial reader!");

    // Bad things are gonna happen if this is null
    configASSERT(usb_serial_incoming_commands != NULL);

    gpio_init(CDC_ACTIVE_PIN);
    gpio_set_dir(CDC_ACTIVE_PIN, GPIO_OUT);
    bool isOn = true;
    uint8_t direction = 1;
    uint64_t count = 0L;


    // Define our buffer and zero it out
    char* rx_buffer = (char *) pvPortMalloc(sizeof(char) * USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);
    memset(rx_buffer, '\0', USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);


#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for (EVER) {

        if (xQueueReceive(usb_serial_incoming_commands, rx_buffer, (TickType_t) portMAX_DELAY) == pdPASS) {

            serial_messages_received = serial_messages_received + 1;

            // Create a buffer to hold the message and null it out
            char message[USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH];
            memset(message, '\0', USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);

            // Copy the message into the buffer
            strncpy(message, rx_buffer, USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);

            // Send this off to the message processor
            processMessage(message);

            // Wipe out the buffer for next time
            memset(rx_buffer, '\0', USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);

            // Make the LED on the board blink
            if (isOn) {
                direction = 1;
            } else {
                direction = 0;
            }
            gpio_put(CDC_ACTIVE_PIN, direction);
            isOn = !isOn;
        }
    }
#pragma clang diagnostic pop

}


portTASK_FUNCTION(outgoing_serial_writer_task, pvParameters) {

    debug("hello from the serial writer!");

    // Bad things are gonna happen if this is null, too! 😱
    configASSERT(usb_serial_outgoing_messages != NULL);

    // Allocate buffer on the heap
    char *rx_buffer = (char *)pvPortMalloc(USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH + 3);
    if (rx_buffer == NULL) {
        configASSERT("stopping the serial writer because of a failure to allocate memory");
        return;
    }

    memset(rx_buffer, '\0', USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH + 2);

    for (EVER) {

        // Make sure that we aren't writing anything bigger than this on the other side!
        if (xQueueReceive(usb_serial_outgoing_messages, rx_buffer, portMAX_DELAY) == pdPASS) {

            serial_messages_sent = serial_messages_sent + 1;

            size_t messageLength = strlen(rx_buffer);

            // Ensure that the messaging is null-terminated
            rx_buffer[messageLength] = '\n';
            rx_buffer[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH + 1] = '\0';

            cdc_send(rx_buffer);

            memset(rx_buffer, '\0', USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH + 2);
        }
    }

}
