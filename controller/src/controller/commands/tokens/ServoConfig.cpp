
#include <cmath>
#include <string>

#include "controller-config.h"
#include "creature/MotorType.h"
#include "device/Servo.h"
#include "logging/Logger.h"

#include "ServoConfig.h"

namespace creatures {

ServoConfig::ServoConfig(const std::shared_ptr<Logger> &logger, const std::shared_ptr<Servo> &servo)
    : logger(logger), servo(servo) {

    logger->debug("ServoConfig token made for servo on module {} at location {}",
                  config::UARTDevice::moduleNameToString(servo->getOutputModule()), servo->getOutputHeader());
}

u16 ServoConfig::getOutputHeader() const { return servo->getOutputHeader(); }

ServoSpecifier ServoConfig::getOutputLocation() const { return servo->getOutputLocation(); }

std::string ServoConfig::toString() const {

    if (servo->getMotorType() == creatures::creature::motor_type::dynamixel) {
        // Dynamixel: compute profile velocity from smoothing value
        u32 velocity = static_cast<u32>(std::round((1.0f - servo->getSmoothingValue()) * DXL_MAX_PROFILE_VELOCITY));
        std::string message = fmt::format("DYNAMIXEL {} {} {} {}", servo->getOutputHeader(), servo->getMinPulseUs(),
                                          servo->getMaxPulseUs(), velocity);
        logger->debug("ServoConfig message is: {}", message);
        return message;
    }

    // PWM servo — existing format
    std::string message =
        fmt::format("SERVO {} {} {}", servo->getOutputHeader(), servo->getMinPulseUs(), servo->getMaxPulseUs());
    logger->debug("ServoConfig message is: {}", message);
    return message;
}

} // namespace creatures