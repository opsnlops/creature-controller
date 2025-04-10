#include <sys/cdefs.h>
#include <hardware/watchdog.h>

#include <FreeRTOS.h>
#include <timers.h>

#include "controller/config.h"
#include "logging/logging.h"
#include "watchdog.h"


/**
 * @brief Initialize the watchdog timer
 *
 * @return true if watchdog started successfully, false otherwise
 */
bool init_watchdog(void) {
    // Start the watchdog timer with the configured timeout
    watchdog_enable(WATCHDOG_TIMEOUT_MS, 1);
    watchdog_update();

    return true;
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