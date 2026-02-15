

#include <string>
#include <vector>

#include <fmt/format.h>

#include "controller/commands/tokens/ServoPosition.h"
#include "creature/MotorType.h"

#include "CommandException.h"
#include "SetServoPositions.h"

namespace creatures::commands {

SetServoPositions::SetServoPositions(std::shared_ptr<Logger> logger) : logger(logger) { // NOLINT(*-pass-by-value)
    this->servoPositions = std::vector<ServoPosition>();
    this->filter = creatures::config::UARTDevice::invalid_module;
}

void SetServoPositions::addServoPosition(const creatures::ServoPosition &servoPosition) {

    // Make sure we're not putting the same outputPosition in twice
    for (const auto &existingServoPosition : servoPositions) {
        if (existingServoPosition.getServoId() == servoPosition.getServoId()) {
            const auto errorMessage = fmt::format(
                "Unable to insert the same output position twice: module {}, pin {}, type {}",
                creatures::config::UARTDevice::moduleNameToString(servoPosition.getServoId().module),
                servoPosition.getServoId().pin,
                servoPosition.getServoId().type == creatures::creature::motor_type::dynamixel ? "dynamixel" : "servo");
            logger->error(errorMessage);
            throw CommandException(errorMessage);
        }
    }

    this->servoPositions.push_back(servoPosition);
    logger->trace("Added servo position: {}", servoPosition.toString());
}

std::string SetServoPositions::toMessage() {

    // Yell if we're doing this on a blank set of positions
    if (servoPositions.empty()) {
        logger->warn("attempted to call toMessage() on an empty SetServoPositions");
        return "";
    }

    // Start the message with the 'POS' command prefix
    std::string message = "POS";

    // Now go make the string
    for (const auto &position : servoPositions) {
        message += '\t' + position.toString();
    }

    logger->trace("message is: {}", message);
    return message;
}

} // namespace creatures::commands
