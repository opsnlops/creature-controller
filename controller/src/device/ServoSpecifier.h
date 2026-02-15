
#pragma once

#include <functional>

#include "config/UARTDevice.h"
#include "creature/MotorType.h"

/**
 * This struct is how servo devices are identified
 */
struct ServoSpecifier {
    creatures::config::UARTDevice::module_name module;
    u16 pin; // GPIO pin for PWM, Dynamixel bus ID for dynamixel
    creatures::creature::motor_type type;

    ServoSpecifier(creatures::config::UARTDevice::module_name _module, u16 _pin,
                   creatures::creature::motor_type _type = creatures::creature::motor_type::servo)
        : module(_module), pin(_pin), type(_type) {}

    // Allow for direct comparision
    friend bool operator==(const ServoSpecifier &lhs, const ServoSpecifier &rhs) {
        return lhs.module == rhs.module && lhs.pin == rhs.pin && lhs.type == rhs.type;
    }
};

/**
 * Make sure this can be hashed for use in maps
 */
namespace std {
template <> struct hash<ServoSpecifier> {
    size_t operator()(const ServoSpecifier &ss) const {
        return hash<creatures::config::UARTDevice::module_name>()(ss.module) ^ (hash<u16>()(ss.pin) << 1) ^
               (hash<int>()(static_cast<int>(ss.type)) << 2);
    }
};
} // namespace std
