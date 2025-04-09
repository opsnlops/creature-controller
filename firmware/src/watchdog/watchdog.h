#include <sys/cdefs.h>

#pragma once

#include <stdbool.h>
#include <hardware/watchdog.h>
#include <FreeRTOS.h>
#include <timers.h>

/**
 * @brief Start the watchdog timer with configured timeout
 *
 * Initializes the hardware watchdog and creates a FreeRTOS timer to
 * periodically reset it. If the timer fails to reset the watchdog
 * within WATCHDOG_TIMEOUT_MS, the system will reboot.
 *
 * @return true if watchdog was successfully started, false otherwise
 */
bool start_watchdog_timer(void);

/**
 * @brief FreeRTOS timer callback to reset the watchdog
 *
 * This function is called periodically by the FreeRTOS timer to
 * feed the watchdog and prevent a system reset.
 *
 * @param xTimer Handle to the timer that triggered this callback
 */
void watchdog_timer_callback(TimerHandle_t xTimer);

/**
 * @brief Intentionally force a system reboot using the watchdog
 *
 * Enables watchdog with minimum timeout and enters infinite loop
 * to ensure the system resets.
 */ _Noreturn
void reboot(void);