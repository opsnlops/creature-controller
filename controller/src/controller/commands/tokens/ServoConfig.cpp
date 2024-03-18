
#include <string>

#include "device/Servo.h"
#include "logging/Logger.h"

#include "ServoConfig.h"


namespace creatures {

    ServoConfig::ServoConfig(std::shared_ptr<Logger> logger, std::shared_ptr<Servo> servo) :
            logger(logger), servo(servo) {

        logger->debug("ServoConfig token made for servo on module {} at location {}",
                      creatures::config::UARTDevice::moduleNameToString(servo->getOutputModule()),
                      servo->getOutputHeader());

    }

    u16 ServoConfig::getOutputHeader() const {
        return servo->getOutputHeader();
    }

    std::string ServoConfig::toString() const {

        // Let's make the string for this servo!
        std::string message = fmt::format("SERVO {} {} {}",
                                          servo->getOutputHeader(), servo->getMinPulseUs(), servo->getMaxPulseUs());
        logger->debug("ServoConfig message is: {}", message);
        return message;
    }

} // creatures