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

/**
 * @brief Check whether an emergency stop has been activated
 *
 * Once set this never clears — only a power cycle recovers. Tasks that can put
 * the creature back in motion must check this before doing so, since the
 * emergency stop halts the message processor but not the rest of the system.
 *
 * @return true if emergency stop has been activated
 */
bool is_emergency_stop_active(void);
