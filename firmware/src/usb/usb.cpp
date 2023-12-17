
#include "controller-config.h"

#include <FreeRTOS.h>
#include <task.h>

#include "logging/logging.h"
#include "usb/usb.h"

u32 reports_sent = 0;
bool usb_bus_active = false;
bool device_mounted = false;
u32 events_processed = 0;


TaskHandle_t usb_device_task_handle;


void start_usb_tasks() {

    // Create a task for tinyusb device stack
    xTaskCreate(usb_device_task,
                "usbd",
                2048,
                nullptr,
                1,
                &usb_device_task_handle);
}


//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    debug("device mounted");
    device_mounted = true;
    usb_bus_active = true;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    debug("device unmounted");
    device_mounted = false;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    debug("USB bus suspended");

    device_mounted = false;
    usb_bus_active = false;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    debug("USB bus resumed");
    usb_bus_active = true;
}


/*
 * CDC Stuff
 */

void cdc_send(char const* message) {

    if (tud_cdc_connected()) {

        tud_cdc_n_write_str(0, message);
        tud_cdc_n_write_flush(0);
    }
    else {
        verbose("skipped CDC send");
    }
}



// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
_Noreturn void usb_device_task(void *param) {
    (void) param;

    tusb_init();

    // init device stack on configured roothub port
    // This should be called after scheduler/kernel is started.
    // Otherwise, it could cause kernel issue since USB IRQ handler does use RTOS queue API.
    tud_init(BOARD_TUD_RHPORT);

    // RTOS forever loop
    for (EVER) {
        // put this thread to waiting state until there is new events
        tud_task();

        // Give the RTOS a chance to breathe, USB is not the primary thing we do
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}