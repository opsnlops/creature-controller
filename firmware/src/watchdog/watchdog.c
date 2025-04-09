#include <sys/cdefs.h>
#include <hardware/watchdog.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "controller/config.h"
#include "logging/logging.h"
#include "watchdog.h"

// Handle for the watchdog timer
static TimerHandle_t watchdog_timer = NULL;

/**
 * @brief Start the watchdog timer
 *
 * @return true if watchdog started successfully, false otherwise
 */
bool start_watchdog_timer(void) {
    // Start the watchdog timer with the configured timeout
    watchdog_enable(WATCHDOG_TIMEOUT_MS, 1);
    watchdog_update();

    // Create a timer to update the watchdog periodically
    watchdog_timer = xTimerCreate(
            "watchdog_timer",
            pdMS_TO_TICKS(WATCHDOG_TIMER_PERIOD_MS),
            pdTRUE,                        // Auto-reload
            NULL,                          // Timer ID
            watchdog_timer_callback        // Callback function
    );

    // Check if timer creation was successful
    if (watchdog_timer == NULL) {
        error("Failed to create watchdog timer");
        return false;
    }

    // Start the timer with a timeout to ensure it starts
    if (xTimerStart(watchdog_timer, pdMS_TO_TICKS(100)) != pdPASS) {
        error("Failed to start watchdog timer");
        return false;
    }

    info("Watchdog timer started (timeout: %u ms, period: %u ms)",
         WATCHDOG_TIMEOUT_MS, WATCHDOG_TIMER_PERIOD_MS);
    return true;
}

/**
 * @brief Watchdog timer callback function
 *
 * Called periodically to feed the watchdog and prevent system reset.
 *
 * @param xTimer The timer handle (unused)
 */
void watchdog_timer_callback(TimerHandle_t xTimer) {
    (void) xTimer; // Prevent unused parameter warning

    // Reset the watchdog to prevent system reset
    watchdog_update();

    // Could add additional system checks here in the future
}

/**
 * @brief Force a system reboot using the watchdog
 *
 * Sets a very short watchdog timeout and enters an infinite loop,
 * ensuring the system will reset quickly.
 */
_Noreturn void reboot(void) {
    info("Initiating system reboot via watchdog");

    // Disable interrupts using FreeRTOS API
    taskDISABLE_INTERRUPTS();

    // Configure watchdog for immediate reset (1ms timeout)
    watchdog_enable(1, 1);

    // Enter infinite loop to ensure reset occurs
    for(EVER) {
        // Wait for watchdog reset
    }
}