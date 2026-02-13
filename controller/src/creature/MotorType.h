
#pragma once

namespace creatures::creature {

/**
 * @brief Motor types supported by the system
 *
 * Shared between Creature, Servo, and ServoSpecifier to avoid
 * circular includes.
 */
enum motor_type { servo, dynamixel, stepper, invalid_motor };

} // namespace creatures::creature
