
#include <stddef.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "controller/config.h"

#include "io/usb_serial.h"
#include "logging/logging.h"
#include "usb/usb.h"

#include "types.h"

// Stats (from the status_lights module)
extern volatile u64 usb_serial_characters_received;

// From message_processor.c - the inbound pipeline never blocks; it drops and
// counts. tud_cdc_rx_cb feeds the first queue in that pipeline.
extern volatile u64 incoming_messages_dropped;

TaskHandle_t incoming_usb_serial_reader_task_handle;

QueueHandle_t usb_serial_incoming_commands;
QueueHandle_t usb_serial_outgoing_messages;

bool incoming_usb_queue_exists = false;
bool outgoing_usb_queue_exists = false;

void usb_serial_init() {

    // Create the incoming queue
    usb_serial_incoming_commands =
        xQueueCreate(USB_SERIAL_INCOMING_QUEUE_LENGTH, USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);
    vQueueAddToRegistry(usb_serial_incoming_commands, "usb_incoming_queue");
    incoming_usb_queue_exists = true;

    // And the outgoing queue
    usb_serial_outgoing_messages =
        xQueueCreate(USB_SERIAL_OUTGOING_QUEUE_LENGTH, USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);
    vQueueAddToRegistry(usb_serial_outgoing_messages, "usb_outgoing_queue");
    outgoing_usb_queue_exists = true;

    info("created the USB serial queues");
}

void usb_serial_start() {

    info("starting the incoming USB serial reader task");
    xTaskCreate(incoming_usb_serial_reader_task, "incoming_usb_serial_reader_task", 1512, NULL,
                INCOMING_RX_TASK_PRIORITY, &incoming_usb_serial_reader_task_handle);

    // The outbound side is handled by usb_device_task (see usb.c): TinyUSB
    // OPT_OS_NONE requires a single context for tud_task() + CDC writes, so
    // there is no separate writer task anymore.
}

// (Removed is_safe_to_enqueue_incoming/outgoing_usb_serial: dead code that
//  also misused xQueueIsQueueFullFromISR from task context. The pipeline now
//  uses non-blocking sends and acts on their return value instead.)

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

    // Drain the CDC RX FIFO in fixed-size chunks. This callback runs inside
    // tud_task() on the timer-daemon stack, so a FIFO-sized VLA here (the RX
    // FIFO is CFG_TUD_CDC_RX_BUFSIZE = 8 KB) would overflow that stack. A
    // bounded buffer + loop drains everything with no unbounded stack use.
    u8 tempBuffer[USB_SERIAL_RX_CHUNK];
    u32 readCount;

    while ((readCount = tud_cdc_read(tempBuffer, sizeof(tempBuffer))) > 0) {

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
            usb_serial_characters_received = usb_serial_characters_received + 1;

            // Is this our reset character?
            if (ch == 0x07) {

                // We heard a bell! Time to reset everything!
                memset(lineBuffer, '\0', sizeof(lineBuffer));
                bufferIndex = 0;

                // Let the user know, if the logger is up and running! 😅
                info("We heard the bell! The incoming buffer has been reset!");

            }

            // Check for newline character
            else if (ch == 0x0A) {

                verbose("it's the blessed character");

                // If there was a blank line warn the sender. Use continue, not
                // break: break would discard every byte already read after
                // this point in the chunk (those bytes are gone from the FIFO),
                // silently dropping any commands that followed the blank line.
                if (bufferIndex == 0) {
                    warning("skipping blank input line from sender");
                    continue;
                }

                lineBuffer[bufferIndex] = '\0'; // Null-terminate the string

                verbose("queue length: %u", uxQueueMessagesWaiting(usb_serial_incoming_commands));
                // tud_cdc_rx_cb runs inside tud_task() (the timer-daemon task),
                // never an ISR - so use the task API, and never block: drop and
                // count if the command queue is full.
                if (xQueueSendToBack(usb_serial_incoming_commands, lineBuffer, 0) != pdTRUE)
                    incoming_messages_dropped++;
                verbose("queue length: %u", uxQueueMessagesWaiting(usb_serial_incoming_commands));

                bufferIndex = 0; // Reset buffer index

            } else if (bufferIndex < USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH - 1) {
                lineBuffer[bufferIndex++] = ch; // Store character and increment index

            } else {
                // Buffer overflow handling
                lineBuffer[USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH - 1] = '\0'; // Ensure null-termination
                if (xQueueSendToBack(usb_serial_incoming_commands, lineBuffer, 0) != pdTRUE)
                    incoming_messages_dropped++;

                bufferIndex = 0; // Reset buffer index

                warning("buffer overflow on incoming data");
            }
        }
    }

    // Additional safety check for null-termination
    lineBuffer[USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH - 1] = '\0';
}
