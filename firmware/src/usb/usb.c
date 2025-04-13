/**
 * @file usb.c
 * @brief USB subsystem management for creature controller
 *
 * This module handles USB device initialization, monitoring connection status,
 * and CDC communication for the creature controller firmware.
 */

#include <stddef.h>

#include <FreeRTOS.h>
#include <timers.h>

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

/**
 * @brief Initialize the USB subsystem
 *
 * This function initializes the TinyUSB stack and configures the device
 * stack on the root hub port. It also sets up the LED indicator for USB status.
 * Note that this should be called after the RTOS scheduler is started since
 * the USB IRQ handler uses RTOS queue APIs.
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
 * @brief Start USB service timers
 *
 * Creates and starts two FreeRTOS timers:
 * 1. Device service timer: runs at 1ms interval to call tud_task()
 * 2. CDC connection monitor timer: runs at 100ms interval to check connection status
 *
 * These timers handle USB stack processing and connection state monitoring
 * without blocking the main application tasks.
 */
void usb_start() {
    // Create timer for USB device tasks (runs every 1ms)
    TimerHandle_t usbDeviceTimer = xTimerCreate(
            "usbDeviceTimer",           // Timer name
            pdMS_TO_TICKS(1),           // Every millisecond
            pdTRUE,                     // Auto-reload
            (void *) 0,                 // Timer ID (not used here)
            usbDeviceTimerCallback      // Callback function
    );

    // Create timer to monitor CDC connection status (runs every 100ms)
    TimerHandle_t cdcConnectedTimer = xTimerCreate(
            "cdcConnectedTimer",        // Timer name
            pdMS_TO_TICKS(100),         // Every 100ms
            pdTRUE,                     // Auto-reload
            (void *) 0,                 // Timer ID (not used here)
            is_cdc_connected_timer      // Callback function
    );

    // Verify timer creation was successful
    configASSERT(usbDeviceTimer != NULL);

    // Start both timers
    xTimerStart(usbDeviceTimer, 0);
    xTimerStart(cdcConnectedTimer, 0);

    info("USB service timer started");
}

/**
 * @brief USB device timer callback
 *
 * This callback is invoked every 1ms to process TinyUSB tasks.
 * It ensures the USB stack gets regular processing time without
 * blocking other operations.
 *
 * @param xTimer Timer handle (unused)
 */
void usbDeviceTimerCallback(TimerHandle_t xTimer) {
    tud_task();  // Process TinyUSB tasks
}

/**
 * @brief CDC connection monitoring timer callback
 *
 * Checks if the CDC connection state has changed and updates the LED indicator.
 * When connection state changes, it notifies the controller module so it can
 * take appropriate action (e.g., start/stop tasks that depend on CDC).
 *
 * @param xTimer Timer handle (unused)
 */
void is_cdc_connected_timer(TimerHandle_t xTimer) {
    if (tud_cdc_connected()) {
        // CDC is connected
        gpio_put(USB_MOUNTED_LED_PIN, true);  // Turn on LED

        // If this is a new connection (state change)
        if (!cdc_connected) {
            debug("CDC connected");
            cdc_connected = true;
            controller_connected();  // Notify controller of connection
        }
    } else {
        // CDC is disconnected
        gpio_put(USB_MOUNTED_LED_PIN, false);  // Turn off LED

        // If this is a new disconnection (state change)
        if (cdc_connected) {
            debug("CDC disconnected");
            cdc_connected = false;
            controller_disconnected();  // Notify controller of disconnection
        }
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
void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;  // Unused parameter
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

//--------------------------------------------------------------------+
// MARK: - CDC Functions
//--------------------------------------------------------------------+

/**
 * @brief Send a string message over CDC
 *
 * Sends a null-terminated string message over the CDC interface if connected.
 * If not connected, the message is skipped and a verbose log entry is generated.
 *
 * @param message Null-terminated string message to send
 */
void cdc_send(char const *message) {
    if (tud_cdc_connected()) {
        tud_cdc_n_write_str(0, message);
        tud_cdc_n_write_flush(0);
    } else {
        verbose("skipped CDC send");
    }
}