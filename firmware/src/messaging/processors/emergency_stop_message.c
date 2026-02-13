/**
 * @file emergency_stop_message.c
 * @brief Emergency stop message processor for animatronic controllers
 *
 * This module handles emergency stop (ESTOP) messages that immediately power down
 * all servo motors and place the system in a safe, non-recoverable state. The
 * emergency stop is designed to be triggered when dangerous conditions are detected
 * such as overheating, excessive power draw, or motor obstructions that require
 * human intervention. Only a power cycle can restore normal operation.
 */

#include <FreeRTOS.h>
#include <task.h>

#include "controller/config.h"
#include "controller/controller.h"
#include "device/power_control.h"
#include "logging/logging.h"
#include "messaging/messaging.h"

// Emergency stop state flag
static volatile bool emergency_stop_active = false;

bool handleEmergencyStopMessage(const GenericMessage *msg) {
    fatal("EMERGENCY STOP ACTIVATED - powering down all motors");

    // Set an emergency stop flag
    emergency_stop_active = true;

#ifdef CC_VER4
    // Disable Dynamixel torque before powering off
    dynamixel_set_torque_all(false);
#endif

#ifdef CC_VER3
    // Immediately power off all motors
    if (!disable_all_motors()) {
        error("failed to disable all motors during emergency stop");
    }
#endif

#ifdef CC_VER2
    // If we're on version 2, it's not possible to power down the motors. Still halt the event
    // loop, and warn that we can't actually power them off.
    fatal("CC_VER2 hardware detected - cannot power off motors, manual power disconnect required");
#endif

    fatal("emergency stop complete - system waiting for power cycle");

    // Enter infinite wait loop - only power cycle will recover. This will halt the
    // message processor because we don't want to process any more messages
    // after an emergency stop.
    for (EVER) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        warning("system in emergency stop state - waiting for power cycle");
    }

    // Should never reach here
    return true;
}

/**
 * @brief Check if the emergency stop is currently active
 *
 * @return true if emergency stop has been activated
 */
bool is_emergency_stop_active(void) { return emergency_stop_active; }
