
#include <string>
#include <vector>

#include "device/Servo.h"
#include "logging/Logger.h"

#include "ServoConfig.h"
#include "controller-config.h"


namespace creatures {

    ServoConfig::ServoConfig(std::shared_ptr<Logger> logger, std::shared_ptr<Servo> servo) :
            logger(logger), servo(servo) {

        logger->debug("ServoConfig token made for servo {}", servo->getId());

    }

    std::string ServoConfig::getOutputPosition() const {
        return servo->getOutputLocation();
    }

    std::string ServoConfig::toString() const {

        // Let's make the string for this servo!
        std::string message = fmt::format("SERVO {} {} {}",
                                          servo->getOutputLocation(), servo->getMinPulseUs(), servo->getMaxPulseUs());
        logger->debug("ServoConfig message is: {}", message);
        return message;
    }

} // creatures