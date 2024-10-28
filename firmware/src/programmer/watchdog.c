

#include <hardware/watchdog.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "config.h"
#include "logging/logging.h"
#include "watchdog.h"

void start_watchdog_timer() {
    // Start the watchdog timer
    watchdog_enable(WATCHDOG_TIMEOUT_MS, 1);
    watchdog_update();

    // Create a timer to update the watchdog
    TimerHandle_t watchdog_timer = xTimerCreate(
            "watchdog_timer",
            pdMS_TO_TICKS(WATCHDOG_TIMER_PERIOD_MS),
            pdTRUE,                        // Auto-reload
            NULL,                          // Timer ID
            watchdog_timer_callback        // Callback function
    );

    // Start the timer
    xTimerStart(watchdog_timer, 0);

    info("watchdog timer started");
}



void watchdog_timer_callback(TimerHandle_t xTimer) {
    (void) xTimer;

    // Reset the watchdog
    watchdog_update();
}


void reboot() {
    watchdog_enable(1, 1);
    for(EVER) {};
}