
#include <stddef.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "logging/logging.h"

#include "config.h"
#include "shell.h"
#include "status_led.h"
#include "types.h"
#include "usb.h"

u32 reports_sent = 0;
bool usb_bus_active = false;
bool device_mounted = false;
u32 events_processed = 0;

bool cdc_connected = false;

extern TaskHandle_t shell_task_handle;

void usb_init() {

    tusb_init();

    tud_init(BOARD_TUD_RHPORT);

    status_led_init();
    cdc_connected = false;
}

void usb_start() {

    TimerHandle_t usbDeviceTimer =
        xTimerCreate("usbDeviceTimer", pdMS_TO_TICKS(1), pdTRUE, (void *)0, usbDeviceTimerCallback);

    TimerHandle_t cdcConnectedTimer =
        xTimerCreate("cdcConnectedTimer", pdMS_TO_TICKS(100), pdTRUE, (void *)0, is_cdc_connected_timer);

    TimerHandle_t clearDataTransmissionLightsTimer =
        xTimerCreate("clearDataTransmissionLightsTimer", pdMS_TO_TICKS(300), pdTRUE, (void *)0,
                     clear_data_transmission_lights_timer);

    configASSERT(usbDeviceTimer != NULL);
    xTimerStart(usbDeviceTimer, 0);
    xTimerStart(cdcConnectedTimer, 0);
    xTimerStart(clearDataTransmissionLightsTimer, 0);

    info("USB service timer started");
}

void usbDeviceTimerCallback(TimerHandle_t xTimer) { tud_task(); }

void clear_data_transmission_lights_timer(TimerHandle_t xTimer) {
    status_led_set(STATUS_LED_INCOMING, false);
    status_led_set(STATUS_LED_OUTGOING, false);
}

void is_cdc_connected_timer(TimerHandle_t xTimer) {

    if (tud_cdc_connected()) {

        status_led_set(STATUS_LED_MOUNTED, true);

        if (!cdc_connected) {
            debug("CDC connected");
            cdc_connected = true;
            launch_shell();
        }

    } else {
        status_led_set(STATUS_LED_MOUNTED, false);

        if (cdc_connected) {
            debug("CDC disconnected");
            cdc_connected = false;
            terminate_shell();
        }
    }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

void tud_mount_cb(void) {
    debug("device mounted");
    device_mounted = true;
    usb_bus_active = true;
}

void tud_umount_cb(void) {
    debug("device unmounted");
    device_mounted = false;
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    debug("USB bus suspended");
    device_mounted = false;
    usb_bus_active = false;
}

void tud_resume_cb(void) {
    debug("USB bus resumed");
    usb_bus_active = true;
}

/*
 * CDC Stuff
 */

void cdc_send(char const *message) {

    if (tud_cdc_connected()) {
        status_led_set(STATUS_LED_OUTGOING, true);
        tud_cdc_n_write_str(0, message);
        tud_cdc_n_write_flush(0);
        debug("sent CDC message: %s", message);
    } else {
        verbose("skipped CDC send");
    }
}
