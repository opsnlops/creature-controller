
#pragma once

#include <functional>

#include "config/UARTDevice.h"

/**
 * This struct is how servo devices are identified
 */
struct ServoSpecifier {
    creatures::config::UARTDevice::module_name module;
    u16 pin;

    ServoSpecifier(creatures::config::UARTDevice::module_name module, u16 pin)
        : module(module), pin(pin) {}

    // Allow for direct comparision
    friend bool operator==(const ServoSpecifier &lhs,
                           const ServoSpecifier &rhs) {
        return lhs.module == rhs.module && lhs.pin == rhs.pin;
    }
};

/**
 * Make sure this can be hashed for use in maps
 */
namespace std {
template <> struct hash<ServoSpecifier> {
    size_t operator()(const ServoSpecifier &ss) const {
        return hash<creatures::config::UARTDevice::module_name>()(ss.module) ^
               hash<u16>()(ss.pin) << 1; // Combining hashes of module and pin
    }
};
} // namespace std
