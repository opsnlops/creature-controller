
#include <string>
#include <utility>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tusb_config.h"

#include "logging/logging.h"
#include "io/usb_serial.h"

#include "tasks.h"
#include "usb/usb.h"

#define DS_TX_BUFFER_SIZE       1024
#define DS_RX_BUFFER_SIZE       16

QueueHandle_t debug_shell_incoming_keys;

/**
 * Invoked from TUSB when there's data to be read
 *
 * This prevents us from having to poll, which is nice!
 *
 * @param itf the CDC interface number
 */
void tud_cdc_rx_cb(uint8_t itf) {
    verbose("callback from tusb that there's data there");
    uint8_t ch = tud_cdc_read_char();

    // Make sure it's not a control character that accidentally ended up in there
    if(ch >= 30  && ch <= 126)
        xQueueSendToBackFromISR(debug_shell_incoming_keys, &ch, nullptr);
    else {
        info("discarding character from shell: 0x%x", ch);
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
        if (xQueueReceive(debug_shell_incoming_keys, &ch, (TickType_t) portMAX_DELAY) == pdPASS) {

            // Look at just the first keypress
            rx_buffer[0] = ch;

            cdc_send(std::string(rx_buffer[0], 1));

            // Wipe out the buffers for next time
            ds_reset_buffers(tx_buffer, rx_buffer);
        }
    }
#pragma clang diagnostic pop

}
