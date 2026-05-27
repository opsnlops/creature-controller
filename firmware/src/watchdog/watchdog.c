#include <hardware/watchdog.h>

#include <FreeRTOS.h>
#include <task.h>

#include "controller/config.h"
#include "logging/logging.h"
#include "types.h"
#include "watchdog.h"

#if USE_WATCHDOG_HEALTH_GATE
/*
 * Liveness flag for the watchdog health gate. Set true by the lowest-priority
 * heartbeat task each time it runs; read-and-cleared by watchdog_feed() in the
 * PWM-wrap ISR. Single aligned bool -> atomic load/store on Cortex-M, so no
 * critical section is needed: the only race (task sets it just as the ISR
 * clears it) merely skips one refresh, recovered on the next ISR cycle, well
 * inside WATCHDOG_TIMEOUT_MS. Seeded true so the brief startup window between
 * init_watchdog() and the heartbeat task's first run is covered (the heartbeat
 * task sets it again within milliseconds of the scheduler starting).
 */
static volatile bool watchdog_health_ok = true;

static TaskHandle_t watchdog_heartbeat_task_handle = NULL;

portTASK_FUNCTION(watchdog_heartbeat_task, pvParameters) {
    (void)pvParameters;
    for (EVER) {
        // Re-arm first, then sleep: the scheduler running this lowest-priority
        // task at all is the liveness proof. Independent of host/comms state.
        watchdog_health_ok = true;
        vTaskDelay(pdMS_TO_TICKS(WATCHDOG_HEARTBEAT_PERIOD_MS));
    }
}
#endif

void watchdog_feed(void) {
#if USE_WATCHDOG_HEALTH_GATE
    // Refresh only if the scheduler proved liveness since the last refresh.
    if (watchdog_health_ok) {
        watchdog_health_ok = false;
        watchdog_update();
    }
#else
    watchdog_update();
#endif
}

void watchdog_health_start(void) {
#if USE_WATCHDOG_HEALTH_GATE
    const BaseType_t ok = xTaskCreate(watchdog_heartbeat_task, "wd_heartbeat", WATCHDOG_HEARTBEAT_TASK_STACK, NULL,
                                      WATCHDOG_HEARTBEAT_TASK_PRIORITY, &watchdog_heartbeat_task_handle);
    configASSERT(ok == pdPASS);
    info("Watchdog health gate enabled (heartbeat every %d ms)", WATCHDOG_HEARTBEAT_PERIOD_MS);
#else
    // Health gate disabled: watchdog_feed() refreshes unconditionally, exactly
    // as the original design. Nothing to start.
#endif
}

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

    // Disable interrupts using FreeRTOS API, which should stop the ISR that updates the counter
    taskDISABLE_INTERRUPTS();

    // Configure watchdog for immediate reset (1ms timeout)
    watchdog_enable(1, 1);

    // Enter infinite loop to ensure reset occurs
    for (EVER) {
        // Wait for watchdog reset
    }
}