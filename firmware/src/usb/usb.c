/**
 * @file usb.c
 * @brief USB subsystem management for creature controller
 *
 * This module handles USB device initialization, monitoring connection status,
 * and CDC communication for the creature controller firmware.
 *
 * TinyUSB is built with CFG_TUSB_OS = OPT_OS_NONE, which provides no
 * cross-context locking for its CDC FIFO / endpoint state. It is only safe
 * when a *single* execution context drives the device stack and performs all
 * CDC writes (the TinyUSB dcd ISR is the only other toucher, and TinyUSB
 * guards that itself). Earlier designs called tud_task() from the timer
 * daemon while writing CDC from a separate task; the resulting race with the
 * USB hardware ISR corrupted in-flight transfers (one message truncated, the
 * next spliced into it). usb_device_task below is now the sole owner of every
 * tud_* call: it pumps tud_task(), drains the outgoing queue, and tracks the
 * connection state - no timers, no mutex, one context.
 */

#include <stddef.h>
#include <string.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include <hardware/gpio.h>

#include "controller/controller.h"
#include "logging/logging.h"
#include "usb/usb.h"

#include "controller/config.h"
#include "types.h"

// USB status tracking
u32 reports_sent = 0;
bool usb_bus_active = false;
bool device_mounted = false;
u32 events_processed = 0;
bool cdc_connected = false;

// Times the CDC TX FIFO could not take a whole message in one write (host
// backpressure). The message is NOT lost - it resumes on the next loop once
// tud_task() has drained the FIFO. A growing value just means a slow host.
volatile u64 usb_tx_fifo_full = 0;

static TaskHandle_t usb_device_task_handle = NULL;

// Outgoing transport queue, created in usb_serial_init().
extern QueueHandle_t usb_serial_outgoing_messages;
extern bool outgoing_usb_queue_exists;

// Stat shared with the STATS reporter (USB_SENT).
extern volatile u64 usb_serial_messages_sent;

// Outbound drop counter (STATS: OUT_DROP). The pipeline never blocks - it
// drops and counts. The USB task adds to this when it discards a stale
// backlog (disconnected) or abandons a wedged message.
extern volatile u64 outgoing_messages_dropped;

portTASK_FUNCTION_PROTO(usb_device_task, pvParameters);

/**
 * @brief Initialize the USB subsystem
 *
 * Initializes the TinyUSB stack and the device stack on the root hub port,
 * and configures the USB-status LED. Must be called after the RTOS scheduler
 * is started since the USB IRQ handler uses RTOS APIs.
 */
void usb_init() {
    // Initialize TinyUSB stack
    tusb_init();

    // Initialize device stack on configured roothub port
    // Must be called after scheduler is started since USB IRQ handler uses RTOS queue APIs
    tud_init(BOARD_TUD_ROOT_HUB_PORT);

    // Configure LED pin to indicate USB connection status
    gpio_init(USB_MOUNTED_LED_PIN);
    gpio_set_dir(USB_MOUNTED_LED_PIN, GPIO_OUT);
    gpio_put(USB_MOUNTED_LED_PIN, false);
    cdc_connected = false;
}

/**
 * @brief Start the single USB device task
 *
 * One task owns the entire device stack: it services tud_task(), transmits
 * queued CDC messages, and monitors the connection. This replaces the old
 * 1 ms / 100 ms timers and the separate outbound writer task.
 */
void usb_start() {
    const BaseType_t ok = xTaskCreate(usb_device_task, "usb_device_task", USB_DEVICE_TASK_STACK_SIZE, NULL,
                                      USB_DEVICE_TASK_PRIORITY, &usb_device_task_handle);
    configASSERT(ok == pdPASS);
    info("USB device task started");
}

/**
 * @brief Reconcile CDC connection state, LED, and controller notification
 *
 * Called only from usb_device_task (same context as tud_task()), so the
 * tud_cdc_connected() read needs no locking.
 */
static void service_cdc_connection(void) {
    const bool connected = tud_cdc_connected();

    gpio_put(USB_MOUNTED_LED_PIN, connected);

    if (connected && !cdc_connected) {
        debug("CDC connected");
        cdc_connected = true;
        controller_connected();
    } else if (!connected && cdc_connected) {
        debug("CDC disconnected");
        cdc_connected = false;
        controller_disconnected();
    }
}

/**
 * @brief The one and only owner of the TinyUSB device stack
 *
 * Loop: service the stack, make non-blocking progress transmitting queued CDC
 * messages (resuming a partially-written message across iterations so nothing
 * is ever truncated or blocked), and periodically reconcile the connection
 * state. Pacing matches the old 1 ms timer cadence and yields every pass.
 */
portTASK_FUNCTION(usb_device_task, pvParameters) {

    (void)pvParameters;

    configASSERT(outgoing_usb_queue_exists);

    // The message currently being transmitted and how far it has been handed
    // to the CDC TX FIFO. tx_len == 0 means "no message in flight".
    static char tx_msg[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH + 1];
    size_t tx_len = 0;
    size_t tx_sent = 0;
    u32 tx_stalls = 0; // consecutive zero-progress writes for the in-flight message

    u32 conn_check_ticks = 0;

    for (EVER) {

        // 1. Service the device stack: enumeration, events, RX callbacks, and
        //    draining the CDC TX FIFO to the host.
        tud_task();

        // 2. Make progress on CDC transmission. Never blocks; single context
        //    so the byte stream stays correctly framed.
        if (tud_cdc_connected()) {

            // Drain up to a bounded burst per tick, while the FIFO accepts it.
            // This keeps the small outgoing queue from filling under normal
            // bursts (BSENSE + 2 LOG + INIT + STATS) without ever spinning.
            for (u32 burst = 0; burst < USB_TX_BURST_MAX; burst++) {

                if (tx_len == 0) {
                    if (xQueueReceive(usb_serial_outgoing_messages, tx_msg, 0) != pdPASS) {
                        break; // nothing queued
                    }
                    tx_msg[USB_SERIAL_OUTGOING_MESSAGE_MAX_LENGTH] = '\0';
                    tx_len = strlen(tx_msg);
                    tx_sent = 0;
                    tx_stalls = 0;
                }

                const u32 n = tud_cdc_n_write(0, tx_msg + tx_sent, tx_len - tx_sent);
                tx_sent += n;

                if (tx_sent >= tx_len) {
                    // Whole message handed to the FIFO, correctly framed.
                    tx_len = 0;
                    usb_serial_messages_sent = usb_serial_messages_sent + 1;
                    continue; // try to drain another from this burst
                }

                // FIFO has no room right now.
                usb_tx_fifo_full = usb_tx_fifo_full + 1;

                if (n == 0 && ++tx_stalls >= USB_TX_STALL_LIMIT) {
                    // This message has made zero progress for too long (host
                    // not draining / endpoint glitch). Drop it - counted - so
                    // one wedged message can't permanently halt all telemetry.
                    // Bounded, never blocks, stream stays framed.
                    outgoing_messages_dropped = outgoing_messages_dropped + 1;
                    tx_len = 0;
                }
                break; // let tud_task() drain the FIFO; resume next tick
            }

            tud_cdc_n_write_flush(0);

        } else {
            // Not connected: don't transmit. Discard the bounded backlog so a
            // reconnect doesn't replay stale state, and count it so the loss
            // shows up in OUT_DROP rather than being silent.
            if (tx_len != 0) {
                outgoing_messages_dropped = outgoing_messages_dropped + 1;
                tx_len = 0;
            }
            while (xQueueReceive(usb_serial_outgoing_messages, tx_msg, 0) == pdPASS) {
                outgoing_messages_dropped = outgoing_messages_dropped + 1;
            }
        }

        // 3. Periodic connection-state reconciliation.
        conn_check_ticks += USB_DEVICE_TASK_POLL_MS;
        if (conn_check_ticks >= USB_CDC_CONNECTION_CHECK_MS) {
            conn_check_ticks = 0;
            service_cdc_connection();
        }

        // 4. Pace the loop and yield. A message split across iterations only
        //    waits this long between chunks, which is plenty given the 8 KB
        //    CDC FIFO and the trickle of telemetry/log traffic.
        vTaskDelay(pdMS_TO_TICKS(USB_DEVICE_TASK_POLL_MS));
    }
}

//--------------------------------------------------------------------+
// MARK: - TinyUSB Device Callbacks
//--------------------------------------------------------------------+

/**
 * @brief Callback when USB device is mounted
 *
 * This function is called by TinyUSB when the device is successfully enumerated
 * by the host. It updates the device state flags to indicate the device is active.
 */
void tud_mount_cb(void) {
    debug("device mounted");
    device_mounted = true;
    usb_bus_active = true;
}

/**
 * @brief Callback when USB device is unmounted
 *
 * Called by TinyUSB when the device is detached or the host disconnects.
 * Updates the device state to indicate it's no longer active.
 */
void tud_umount_cb(void) {
    debug("device unmounted");
    device_mounted = false;
}

/**
 * @brief Callback when USB bus is suspended
 *
 * This function is called when the USB bus enters the suspended state.
 * According to USB specs, the device must draw less than 2.5mA from the bus
 * within 7ms of this event.
 *
 * @param remote_wakeup_en True if host allows remote wakeup
 */
void tud_suspend_cb(const bool remote_wakeup_en) {
    (void)remote_wakeup_en; // Unused parameter
    debug("USB bus suspended");

    device_mounted = false;
    usb_bus_active = false;
}

/**
 * @brief Callback when USB bus is resumed
 *
 * Called when the USB bus resumes from suspension. Updates the device state
 * to indicate the bus is active again.
 */
void tud_resume_cb(void) {
    debug("USB bus resumed");
    usb_bus_active = true;
}
