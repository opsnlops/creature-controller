
#include <string>
#include <utility>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "controller-config.h"

#include "tusb_config.h"

#include "logging/logging.h"
#include "io/usb_serial.h"

#include "tasks.h"
#include "usb/usb.h"

#define DS_TX_BUFFER_SIZE       1024
#define DS_RX_BUFFER_SIZE       1024

extern TaskHandle_t incoming_serial_reader_task_handle; // in tasks.cpp
QueueHandle_t usb_serial_incoming_commands;

bool incoming_queue_exists = false;


void usb_serial_init() {
    usb_serial_incoming_commands = xQueueCreate(USB_SERIAL_INCOMING_QUEUE_LENGTH, 20);
    vQueueAddToRegistry(usb_serial_incoming_commands, "usb_serial_incoming_queue_size");
    incoming_queue_exists = true;
}

void start_incoming_usb_serial_reader() {

    info("starting the incoming USB serial reader task");
    xTaskCreate(incoming_serial_reader_task,
                "incoming_serial_reader_task",
                1512,
                nullptr,
                1,
                &incoming_serial_reader_task_handle);
}

bool inline is_safe_to_enqueue_usb_serial() {
    return (incoming_queue_exists && !xQueueIsQueueFullFromISR(usb_serial_incoming_commands));
}

/**
 * Invoked from TUSB when there's data to be read
 *
 * This prevents us from having to poll, which is nice!
 *
 * @param itf the CDC interface number
 */
void tud_cdc_rx_cb(u8 itf) {
    verbose("callback from tusb that there's data there");
    uint8_t ch = tud_cdc_read_char();

    // Make sure it's not a control character that accidentally ended up in there
    if(ch >= 30  && ch <= 126)
        xQueueSendToBackFromISR(usb_serial_incoming_commands, &ch, nullptr);
    else {
        error("discarding character from incoming stream: 0x%x", ch);
        tud_cdc_n_read_flush(itf);
    }
}

void ds_reset_buffers(char *tx_buffer, u8 *rx_buffer) {
    memset(rx_buffer, '\0', DS_RX_BUFFER_SIZE);
    memset(tx_buffer, '\0', DS_TX_BUFFER_SIZE);
}


portTASK_FUNCTION(incoming_serial_reader_task, pvParameters) {

    debug("hello from the debug shell task!");


    auto tx_buffer = (char *) pvPortMalloc(sizeof(char) * DS_TX_BUFFER_SIZE);
    auto rx_buffer = (u8 *) pvPortMalloc(sizeof(u8) * DS_RX_BUFFER_SIZE);   // The pico SDK wants uint8_t

    ds_reset_buffers(tx_buffer, rx_buffer);


#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
    for (EVER) {

        uint8_t ch;
        if (xQueueReceive(usb_serial_incoming_commands, &ch, (TickType_t) portMAX_DELAY) == pdPASS) {

            // Look at just the first keypress
            rx_buffer[0] = ch;

            debug("we received %c", ch);
            cdc_send(std::string(1, rx_buffer[0]));

            // Wipe out the buffers for next time
            ds_reset_buffers(tx_buffer, rx_buffer);
        }
    }
#pragma clang diagnostic pop

}
