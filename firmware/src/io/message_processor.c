
#include <FreeRTOS.h>
#include <queue.h>

#include "io/uart_serial.h"
#include "io/usb_serial.h"
#include "logging/logging.h"
#include "messaging/messaging.h"

#include "message_processor.h"
#include "types.h"

/**
 * @brief Outgoing message queue
 *
 * All messages to be sent to the controller should be added to this queue.
 */
QueueHandle_t outgoing_messages;

/**
 * @brief Incoming message queue
 *
 * Any message that's received, from either interface, will be routed into this
 * queue for processing by the processor task.
 */
QueueHandle_t incoming_messages;

bool incoming_message_queue_exists = false;
bool outgoing_message_queue_exists = false;

TaskHandle_t incoming_message_processor_task_handle;
TaskHandle_t outgoing_message_processor_task_handle;

volatile u64 message_processor_messages_received = 0L;
volatile u64 message_processor_messages_sent = 0L;

// Messages dropped because a pipeline queue was full. The USB pipeline never
// blocks a producer - it drops and counts instead. A non-zero value here means
// the host isn't draining fast enough, not that the pipeline stalled.
volatile u64 outgoing_messages_dropped = 0L;
volatile u64 incoming_messages_dropped = 0L;

// Couple the queue item size to the buffers that feed/drain it, so the two
// constants can never silently diverge into an over-read/overflow.
_Static_assert(INCOMING_MESSAGE_MAX_LENGTH == USB_SERIAL_INCOMING_MESSAGE_MAX_LENGTH,
               "incoming queue item size must match the USB serial incoming buffer size");

// USB Serial queue
extern QueueHandle_t usb_serial_outgoing_messages;
extern bool outgoing_usb_queue_exists;

// UART Serial queue
extern QueueHandle_t uart_serial_outgoing_messages;
extern bool outgoing_uart_queue_exists;

void message_processor_init() {

    // Create the incoming queue
    incoming_messages = xQueueCreate(INCOMING_MESSAGE_QUEUE_LENGTH, INCOMING_MESSAGE_MAX_LENGTH);
    vQueueAddToRegistry(incoming_messages, "incoming_messages");
    incoming_message_queue_exists = true;

    // And the outgoing queue
    outgoing_messages = xQueueCreate(OUTGOING_MESSAGE_QUEUE_LENGTH, OUTGOING_MESSAGE_MAX_LENGTH);
    vQueueAddToRegistry(outgoing_messages, "outgoing_messages");
    outgoing_message_queue_exists = true;

    info("created the message processing");
}

void message_processor_start() {

    info("starting the incoming message processor task");
    xTaskCreate(incoming_message_processor_task, "incoming_message_processor_task", configMINIMAL_STACK_SIZE + 1024,
                NULL, INCOMING_RX_TASK_PRIORITY, &incoming_message_processor_task_handle);

    info("starting the outgoing message processor task");
    xTaskCreate(outgoing_message_processor_task, "outgoing_message_processor_task", configMINIMAL_STACK_SIZE + 1024,
                NULL, 1, &outgoing_message_processor_task_handle);
}

bool send_to_controller(const char *message) {

    const size_t len = strlen(message);

    // Reserve 2 bytes so the '\n' frame + NUL the downstream stages append
    // always fit inside every fixed-size queue slot and buffer in the pipeline.
    if (len > OUTGOING_MESSAGE_MAX_LENGTH - 2) {
        // Log directly to stdio to avoid re-entering send_to_controller() via the
        // logging hook, which would create an infinite error cascade since the
        // LOG-wrapped error message would also exceed the max length.
        printf("[E] not sending message (%u bytes, max %u)\n", (unsigned)len,
               (unsigned)(OUTGOING_MESSAGE_MAX_LENGTH - 2));
        return false;
    }

    if (outgoing_messages == NULL)
        return false;

    // Copy into a slot-sized buffer first. The queue item is
    // OUTGOING_MESSAGE_MAX_LENGTH bytes and xQueueSendToBack copies exactly
    // that many - reading straight from the caller's (often much smaller)
    // buffer would over-read it. Callers include the logging hook, whose
    // buffer is only strlen()+33.
    char slot[OUTGOING_MESSAGE_MAX_LENGTH];
    memset(slot, '\0', sizeof(slot));
    memcpy(slot, message, len);

    // Never block a producer on this queue. A stalled USB pipeline is far
    // worse than a dropped telemetry/log line - especially since producers
    // include the timer daemon, whose stall takes the whole system with it.
    // Enqueue without waiting; drop and count on a full queue.
    if (xQueueSendToBack(outgoing_messages, slot, 0) != pdTRUE) {
        outgoing_messages_dropped++;
        return false;
    }

    return true;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

portTASK_FUNCTION(incoming_message_processor_task, pvParameters) {

    debug("hello from incoming message processor!");

    // Bad things are gonna happen if this is null
    configASSERT(incoming_messages != NULL);

    // Define our buffer and zero it out
    char *rx_buffer = (char *)pvPortMalloc(sizeof(char) * INCOMING_MESSAGE_MAX_LENGTH + 1);
    if (rx_buffer == NULL) {
        configASSERT("unable to allocate a buffer for incoming messages!");
        return;
    }
    memset(rx_buffer, '\0', INCOMING_MESSAGE_MAX_LENGTH + 1);

    for (EVER) {

        if (xQueueReceive(incoming_messages, rx_buffer, (TickType_t)portMAX_DELAY) == pdPASS) {

            message_processor_messages_received = message_processor_messages_received + 1;

            // Create a buffer to hold the message and null it out
            char message[INCOMING_MESSAGE_MAX_LENGTH + 1];
            memset(message, '\0', INCOMING_MESSAGE_MAX_LENGTH + 1);

            // Copy the message into the buffer
            strncpy(message, rx_buffer, INCOMING_MESSAGE_MAX_LENGTH);

            // Process this message
            processMessage(message);

            // Wipe out the buffer for next time
            memset(rx_buffer, '\0', INCOMING_MESSAGE_MAX_LENGTH + 1);
        }
    }
#pragma clang diagnostic pop
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
portTASK_FUNCTION(outgoing_message_processor_task, pvParameters) {

    debug("hello from outgoing message processor!");

    // It's pointless to go on if this is null
    configASSERT(outgoing_messages != NULL);

    // Allocate buffer on the heap
    char *rx_buffer = (char *)pvPortMalloc(OUTGOING_MESSAGE_MAX_LENGTH + 3);
    if (rx_buffer == NULL) {
        configASSERT("unable to allocate a buffer for outgoing messages!");
        return;
    }
    memset(rx_buffer, '\0', OUTGOING_MESSAGE_MAX_LENGTH + 3);

    // Make sure the outgoing queues exist
    if ((void *)outgoing_usb_queue_exists == NULL) {
        configASSERT("outgoing usb queue does not exist!");
        return;
    }

    for (EVER) {

        // Wait for a message, then go!
        if (xQueueReceive(outgoing_messages, rx_buffer, portMAX_DELAY) == pdPASS) {

            message_processor_messages_sent = message_processor_messages_sent + 1;

            size_t messageLength = strlen(rx_buffer);

            // Ensure that the messaging ends with `\n` and is null-terminated
            rx_buffer[messageLength] = '\n';
            rx_buffer[messageLength + 1] = '\0';

            // Copy this into messages for each queue
            char usb_outgoing_message[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH + 1];
            memset(usb_outgoing_message, '\0', USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH + 1);
            strncpy(usb_outgoing_message, rx_buffer, USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);

#ifdef CC_VER2
            char uart_outgoing_message[UART_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH + 1];
            memset(uart_outgoing_message, '\0', UART_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH + 1);
            strncpy(uart_outgoing_message, rx_buffer, UART_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);
#endif

            // Hand off to the transport queue(s) without ever blocking. Using
            // the non-blocking send's return value (instead of a separate
            // FromISR "is it full?" pre-check, which was both a TOCTOU race and
            // a FromISR API misused from task context) keeps the pipeline
            // flowing and makes a full queue a counted drop, not a stall.
            if (xQueueSendToBack(usb_serial_outgoing_messages, usb_outgoing_message, 0) != pdTRUE)
                outgoing_messages_dropped++;

#ifdef CC_VER2
            if (xQueueSendToBack(uart_serial_outgoing_messages, uart_outgoing_message, 0) != pdTRUE)
                outgoing_messages_dropped++;
#endif

            // Wipe out the buffer for next time
            memset(rx_buffer, '\0', OUTGOING_MESSAGE_MAX_LENGTH + 3);
        }
    }
}

#pragma clang diagnostic pop