#include <sys/cdefs.h>

#pragma once

#include <FreeRTOS.h>
#include <hardware/watchdog.h>
#include <stdbool.h>
#include <timers.h>

/**
 * @brief Enable the watchdog timer
 *
 * Initializes the watchdog timer with the timeout defined in config.h. The watchdog
 * it updated in the ISR that runs every time the PWM wraps. If the PWM timer stops
 * working, we're dead in the water, so it's time to reboot.
 *
 * @return true if watchdog was successfully init'ed, false otherwise
 */
bool init_watchdog(void);

/**
 * @brief Refresh the watchdog, applying the health gate
 *
 * Called from the PWM-wrap ISR in place of a raw watchdog_update(). When
 * USE_WATCHDOG_HEALTH_GATE is enabled it refreshes only if the heartbeat task
 * has run since the last refresh (proving the scheduler is alive); otherwise
 * it refreshes unconditionally, exactly as the original design. ISR-safe: no
 * blocking, no FreeRTOS calls.
 */
void watchdog_feed(void);

/**
 * @brief Start the watchdog health-gate heartbeat task
 *
 * Creates the lowest-priority heartbeat task whose execution is the liveness
 * signal watchdog_feed() gates on. No-op when USE_WATCHDOG_HEALTH_GATE is 0.
 * Call once, before the scheduler starts.
 */
void watchdog_health_start(void);

/**
 * @brief Intentionally force a system reboot using the watchdog
 *
 * Enables watchdog with minimum timeout and enters infinite loop
 * to ensure the system resets.
 */
_Noreturn void reboot(void);