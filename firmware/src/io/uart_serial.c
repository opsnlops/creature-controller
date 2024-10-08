
#include <stddef.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "hardware/gpio.h"
#include "hardware/uart.h"

#include "controller-config.h"

#include "logging/logging.h"
#include "io/uart_serial.h"

#include "types.h"


// Stats
volatile u64 uart_characters_received = 0L;
volatile u64 uart_messages_received = 0L;
volatile u64 uart_messages_sent = 0L;

TaskHandle_t incoming_uart_serial_reader_task_handle;
TaskHandle_t outgoing_uart_serial_writer_task_handle;

QueueHandle_t uart_serial_incoming_commands;
QueueHandle_t uart_serial_outgoing_messages;

bool incoming_uart_queue_exists = false;
bool outgoing_uart_queue_exists = false;


// The global incoming messages queue
extern QueueHandle_t incoming_messages;

void uart_serial_init() {

    // Create the incoming queue
    uart_serial_incoming_commands = xQueueCreate(UART_SERIAL_INCOMING_QUEUE_LENGTH,
                                                UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);
    vQueueAddToRegistry(uart_serial_incoming_commands, "uart_incoming_queue");
    incoming_uart_queue_exists = true;

    // And the outgoing queue
    uart_serial_outgoing_messages = xQueueCreate(UART_SERIAL_OUTGOING_QUEUE_LENGTH,
                                                UART_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH);
    vQueueAddToRegistry(uart_serial_outgoing_messages, "uart_outgoing_queue");
    outgoing_uart_queue_exists = true;

    info("created the UART serial queues");

    // Set up the uart
    uart_init((uart_inst_t *) UART_DEVICE_NAME, UART_BAUD_RATE);
    uart_set_format((uart_inst_t *) UART_DEVICE_NAME, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow((uart_inst_t *) UART_DEVICE_NAME, false, false);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);

}

void uart_serial_start() {

    info("starting the incoming UART serial reader task");
    xTaskCreate(incoming_uart_serial_reader_task,
                "incoming_uart_serial_reader_task",
                1512,
                NULL,
                1,
                &incoming_uart_serial_reader_task_handle);

    info("starting the outgoing UART serial writer task");
    xTaskCreate(outgoing_uart_serial_writer_task,
                "outgoing_uart_serial_writer_task",
                1512,
                NULL,
                1,
                &outgoing_uart_serial_writer_task_handle);


    // Register the ISR
    irq_set_exclusive_handler(UART1_IRQ, serial_reader_isr);
    irq_set_enabled(UART1_IRQ, true);
    uart_set_irq_enables(UART_DEVICE_NAME, true, false);
}

bool is_safe_to_enqueue_incoming_uart_serial() {
    return (incoming_uart_queue_exists && !xQueueIsQueueFullFromISR(uart_serial_incoming_commands));
}

bool is_safe_to_enqueue_outgoing_uart_serial() {
    return (outgoing_uart_queue_exists && !xQueueIsQueueFullFromISR(uart_serial_outgoing_messages));
}



portTASK_FUNCTION(incoming_uart_serial_reader_task, pvParameters) {

    debug("hello from the UART serial reader!");

    // Bad things are gonna happen if this is null
    configASSERT(uart_serial_incoming_commands != NULL);


    // Define our buffer and zero it out
    char *rx_buffer = (char *) pvPortMalloc(sizeof(char) * UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);
    if (rx_buffer == NULL) {
        configASSERT("stopping the UART reader because of a failure to allocate memory");
        return;
    }
    memset(rx_buffer, '\0', UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);


#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for (EVER) {

        if (xQueueReceive(uart_serial_incoming_commands, rx_buffer, (TickType_t) portMAX_DELAY) == pdPASS) {

            uart_characters_received = uart_characters_received + 1;

            // Create a buffer to hold the message and null it out
            char message[UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH];
            memset(message, '\0', UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);

            // Copy the message into the buffer
            strncpy(message, rx_buffer, UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);

            // Send this off to the incoming messages queue
            xQueueSendToBack(incoming_messages, &message, (TickType_t) portMAX_DELAY);

            // Wipe out the buffer for next time
            memset(rx_buffer, '\0', UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);

        }
    }
#pragma clang diagnostic pop

}


portTASK_FUNCTION(outgoing_uart_serial_writer_task, pvParameters) {

    debug("hello from the UART serial writer!");

    // Bad things are gonna happen if this is null, too! 😱
    configASSERT(uart_serial_outgoing_messages != NULL);

    // Allocate buffer on the heap
    char *rx_buffer = (char *) pvPortMalloc(UART_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH + 3);
    if (rx_buffer == NULL) {
        configASSERT("stopping the UART writer because of a failure to allocate memory");
        return;
    }

    memset(rx_buffer, '\0', UART_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH + 2);

    for (EVER) {

        // Make sure that we aren't writing anything bigger than this on the other side!
        if (xQueueReceive(uart_serial_outgoing_messages, rx_buffer, portMAX_DELAY) == pdPASS) {

            uart_messages_sent = uart_messages_sent + 1;
            uart_write_blocking(UART_DEVICE_NAME, rx_buffer, strlen(rx_buffer));

            memset(rx_buffer, '\0', UART_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH + 3);
        }
    }

}



/**
 * Much like the USB serial reader, this is handled in an ISR. It's called when
 * there's data to be read, saving time polling.
 */
void __isr serial_reader_isr() {
    static char lineBuffer[UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH];
    static u32 bufferIndex = 0;



    /*
     * Get all of the data we can in this pass, but remember this is an
     * ISR and we need to be done fast!
     */
    while(uart_is_readable(UART_DEVICE_NAME)) {
        u8 ch;  // Temporary buffer to hold incoming data

        ch = uart_getc(UART_DEVICE_NAME);

        // Account for this character
        uart_characters_received = uart_characters_received + 1;

        // Is this our reset character?
        if (ch == 0x07) {

            // We heard a bell! Time to reset everything!
            memset(lineBuffer, '\0', UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH);
            bufferIndex = 0;

            // Let the user know, if the logger is up and running! 😅
            info("We heard the bell! The incoming buffer has been reset!");

        }

        // Check for newline character
        else if (ch == 0x0A) {

            // If there was a blank line warn the sender
            if (bufferIndex == 0) {
                break;
            }

            lineBuffer[bufferIndex] = '\0';  // Null-terminate the string
            xQueueSendToBackFromISR(uart_serial_incoming_commands, lineBuffer, NULL);
            uart_messages_received = uart_messages_received + 1;

            bufferIndex = 0;  // Reset buffer index

        } else if (bufferIndex < UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH - 1) {
            lineBuffer[bufferIndex++] = ch;  // Store character and increment index

        } else {
            // Buffer overflow handling
            lineBuffer[UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH - 1] = '\0';  // Ensure null-termination
            xQueueSendToBackFromISR(uart_serial_incoming_commands, lineBuffer, NULL);
            uart_messages_received = uart_messages_received + 1;

            bufferIndex = 0;  // Reset buffer index

            warning("buffer overflow on incoming data");
        }

    }

    // Additional safety check for null-termination
    lineBuffer[UART_SERIAL_INCOMING_MESSAGE_MAX_LENGTH - 1] = '\0';
}