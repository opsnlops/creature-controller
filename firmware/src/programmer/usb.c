
#include <stddef.h>

#include <FreeRTOS.h>
#include <timers.h>

#include <hardware/gpio.h>

#include "controller/controller.h"
#include "logging/logging.h"
#include "usb/usb.h"

#include "config.h"
#include "controller-config.h"
#include "shell.h"
#include "types.h"
#include "usb.h"

u32 reports_sent = 0;
bool usb_bus_active = false;
bool device_mounted = false;
u32 events_processed = 0;

bool cdc_connected = false;



extern TaskHandle_t shell_task_handle;

void usb_init() {

    // init TinyUSB
    tusb_init();

    // init device stack on configured roothub port
    // This should be called after scheduler/kernel is started.
    // Otherwise, it could cause kernel issue since USB IRQ handler does use RTOS queue API.
    tud_init(BOARD_TUD_RHPORT);

    // Use the on-board LED to show when the device is active
    gpio_init(CDC_MOUNTED_LED_PIN);
    gpio_set_dir(CDC_MOUNTED_LED_PIN, GPIO_OUT);
    gpio_put(CDC_MOUNTED_LED_PIN, false);
    cdc_connected = false;

    // Flash a light when incoming data is received
    gpio_init(INCOMING_LED_PIN);
    gpio_set_dir(INCOMING_LED_PIN, GPIO_OUT);
    gpio_put(INCOMING_LED_PIN, false);

    // Flash a light when incoming data is sent
    gpio_init(OUTGOING_LED_PIN);
    gpio_set_dir(OUTGOING_LED_PIN, GPIO_OUT);
    gpio_put(OUTGOING_LED_PIN, false);

}


void usb_start() {

    TimerHandle_t usbDeviceTimer = xTimerCreate(
            "usbDeviceTimer",              // Timer name
            pdMS_TO_TICKS(1),            // Every millisecond
            pdTRUE,                          // Auto-reload
            (void *) 0,                        // Timer ID (not used here)
            usbDeviceTimerCallback         // Callback function
    );


    TimerHandle_t cdcConnectedTimer = xTimerCreate(
            "cdcConnectedTimer",              // Timer name
            pdMS_TO_TICKS(100),            // Every 100ms
            pdTRUE,                          // Auto-reload
            (void *) 0,                        // Timer ID (not used here)
            is_cdc_connected_timer         // Callback function
    );

    TimerHandle_t clearDataTransmissionLightsTimer = xTimerCreate(
            "clearDataTransmissionLightsTimer",              // Timer name
            pdMS_TO_TICKS(300),            // Every 250ms
            pdTRUE,                          // Auto-reload
            (void *) 0,                        // Timer ID (not used here)
            clear_data_transmission_lights_timer         // Callback function
    );

    // Something's gone really wrong if we can't create the timer
    configASSERT (usbDeviceTimer != NULL);
    xTimerStart(usbDeviceTimer, 0); // Start timers
    xTimerStart(cdcConnectedTimer, 0);
    xTimerStart(clearDataTransmissionLightsTimer, 0);

    info("USB service timer started");

}

void usbDeviceTimerCallback(TimerHandle_t xTimer) {
    tud_task();
}

void clear_data_transmission_lights_timer(TimerHandle_t xTimer) {
    gpio_put(INCOMING_LED_PIN, false);
    gpio_put(OUTGOING_LED_PIN, false);
}

void is_cdc_connected_timer(TimerHandle_t xTimer) {

    if (tud_cdc_connected()) {

        gpio_put(CDC_MOUNTED_LED_PIN, true);

        // Is this a change in state?
        if (!cdc_connected) {
            debug("CDC connected");
            cdc_connected = true;

            launch_shell();
        }

    } else {
        gpio_put(CDC_MOUNTED_LED_PIN, false);

        if(cdc_connected) {
            debug("CDC disconnected");
            cdc_connected = false;

            terminate_shell();

        }
    }
}





//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
    debug("device mounted");
    device_mounted = true;
    usb_bus_active = true;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
    debug("device unmounted");
    device_mounted = false;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;
    debug("USB bus suspended");

    device_mounted = false;
    usb_bus_active = false;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
    debug("USB bus resumed");
    usb_bus_active = true;
}


/*
 * CDC Stuff
 */

void cdc_send(char const *message) {

    if (tud_cdc_connected()) {
        gpio_put(OUTGOING_LED_PIN, true);
        tud_cdc_n_write_str(0, message);
        tud_cdc_n_write_flush(0);
        debug("sent CDC message: %s", message);
    } else {
        verbose("skipped CDC send");
    }
}




