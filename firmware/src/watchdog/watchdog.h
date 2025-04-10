#include <sys/cdefs.h>

#pragma once

#include <stdbool.h>
#include <hardware/watchdog.h>
#include <FreeRTOS.h>
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
 * @brief Intentionally force a system reboot using the watchdog
 *
 * Enables watchdog with minimum timeout and enters infinite loop
 * to ensure the system resets.
 */ _Noreturn
void reboot(void);