
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "controller-config.h"

#include "logging/logging.h"
#include "io/usb_serial.h"
#include "usb/usb.h"


// Stats
volatile u64 serial_characters_received = 0L;

namespace creatures::io::usb_serial {

    TaskHandle_t incoming_serial_reader_task_handle;
    TaskHandle_t outgoing_serial_writer_task_handle;

    QueueHandle_t usb_serial_incoming_commands;
    QueueHandle_t usb_serial_outgoing_messages;

    bool incoming_queue_exists = false;
    bool outgoing_queue_exists = false;

    void init() {

        // Create the incoming queue
        usb_serial_incoming_commands = xQueueCreate(USB_SERIAL_INCOMING_QUEUE_LENGTH,
                                                    USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);
        vQueueAddToRegistry(usb_serial_incoming_commands, "usb_serial_incoming_queue");
        incoming_queue_exists = true;

        // And the outgoing queue
        usb_serial_outgoing_messages = xQueueCreate(USB_SERIAL_OUTGOING_QUEUE_LENGTH,
                                                    USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);
        vQueueAddToRegistry(usb_serial_outgoing_messages, "usb_serial_outgoing_queue");
        outgoing_queue_exists = true;

        info("created the serial queues");

    }

    void start() {

        info("starting the incoming USB serial reader task");
        xTaskCreate(incoming_serial_reader_task,
                    "incoming_serial_reader_task",
                    1512,
                    nullptr,
                    1,
                    &incoming_serial_reader_task_handle);

        info("starting the outgoing USB serial writer task");
        xTaskCreate(outgoing_serial_writer_task,
                    "outgoing_serial_writer_task",
                    1512,
                    nullptr,
                    1,
                    &outgoing_serial_writer_task_handle);
    }

    bool inline is_safe_to_enqueue_incoming_usb_serial() {
        return (incoming_queue_exists && !xQueueIsQueueFullFromISR(usb_serial_incoming_commands));
    }

    bool inline is_safe_to_enqueue_outgoing_usb_serial() {
        return (outgoing_queue_exists && !xQueueIsQueueFullFromISR(usb_serial_outgoing_messages));
    }

    bool send_to_controller(const char* message) {

        if(strlen(message) > USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH) {
            error("not sending messaging %s because it's %u long and the max length is %u",
                  message, strlen(message), USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);
            return false;
        }

        // If the queue isn't full, send it
        if(is_safe_to_enqueue_outgoing_usb_serial()) {
            xQueueSendToBack(usb_serial_outgoing_messages, message, (TickType_t) pdMS_TO_TICKS(10));
            return true;
        }

        return false;
    }

}
/**
 * Invoked from TUSB when there's data to be read
 *
 * This prevents us from having to poll, which is nice!
 *
 * @param itf the CDC interface number
 */
void tud_cdc_rx_cb(uint8_t itf) {
    static char lineBuffer[USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH];
    static u32 bufferIndex = 0;

    // Check how many bytes are available
    u32 count = tud_cdc_available();

    if (count > 0) {
        u8 tempBuffer[count];  // Temporary buffer to hold incoming data
        u32 readCount = tud_cdc_read(tempBuffer, count);

        for (u32 i = 0; i < readCount; ++i) {
            char ch = tempBuffer[i];

#if LOGGING_LEVEL > 4
            // If it's not alphanumeric print the character in hex
            if (isalnum((unsigned char)ch)) {
                verbose("character: %c", ch);
            } else {
                char hexStr[5]; // Enough space for "0xHH\0"
                snprintf(hexStr, sizeof(hexStr), "0x%02X", (unsigned char)ch);
                verbose("character: %s", hexStr);
            }
#endif

            // Account for this character
            serial_characters_received = serial_characters_received + 1;

            // Check for newline character
            if (ch == 0x0A) {

                verbose("it's the blessed character");

                // If there was a blank line warn the sender
                if(bufferIndex == 0) {
                    warning("skipping blank input line from sender");
                    break;
                }

                lineBuffer[bufferIndex] = '\0';  // Null-terminate the string

                verbose("queue length: %u", uxQueueMessagesWaitingFromISR(creatures::io::usb_serial::usb_serial_incoming_commands));
                xQueueSendToBackFromISR(creatures::io::usb_serial::usb_serial_incoming_commands, lineBuffer, nullptr);
                verbose("queue length: %u", uxQueueMessagesWaitingFromISR(creatures::io::usb_serial::usb_serial_incoming_commands));

                bufferIndex = 0;  // Reset buffer index

            } else if (bufferIndex < USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH - 1) {
                lineBuffer[bufferIndex++] = ch;  // Store character and increment index

            } else {
                // Buffer overflow handling
                lineBuffer[USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH - 1] = '\0';  // Ensure null-termination
                xQueueSendToBackFromISR(creatures::io::usb_serial::usb_serial_incoming_commands, lineBuffer, nullptr);

                bufferIndex = 0;  // Reset buffer index

                warning("buffer overflow on incoming data");
            }
        }
    }

    // Additional safety check for null-termination
    lineBuffer[USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH - 1] = '\0';
}
