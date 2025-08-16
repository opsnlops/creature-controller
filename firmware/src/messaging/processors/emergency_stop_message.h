#pragma once

#include "messaging/messaging.h"

/**
 * @brief Handle an emergency stop message
 *
 * Powers off all motors immediately and sets the system into an emergency
 * stop state. System will require a power cycle to resume normal operation.
 *
 * @param msg The ESTOP message (no parameters required)
 * @return true if emergency stop was executed successfully
 */
bool handleEmergencyStopMessage(const GenericMessage *msg);
