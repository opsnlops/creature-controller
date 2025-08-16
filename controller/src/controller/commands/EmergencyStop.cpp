#include "EmergencyStop.h"
#include "controller-config.h"

namespace creatures::commands {

EmergencyStop::EmergencyStop(std::shared_ptr<Logger> logger) : logger(logger) {}

std::string EmergencyStop::toMessage() {
    const std::string message = "ESTOP\t1";  // The 1 is a fake parameter. The parser expects at least one parameter.
    logger->trace("emergency stop message: {}", message);
    return message;
}

} // namespace creatures::commands