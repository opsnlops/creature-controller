

#include <vector>
#include <string>
#include <numeric>

#include <fmt/format.h>

#include "controller-config.h"


#include "controller/commands/tokens/ServoPosition.h"

#include "CommandException.h"
#include "SetServoPositions.h"


namespace creatures {

    SetServoPositions::SetServoPositions(std::shared_ptr<Logger> logger) : logger(logger) { // NOLINT(*-pass-by-value)
        this->servoPositions = std::vector<ServoPosition>();
    }

    void SetServoPositions::addServoPosition(const creatures::ServoPosition &servoPosition) {

        // Make sure we're not putting the same outputPosition in twice
        for (const auto& existingServoPosition : servoPositions) {
            if (existingServoPosition.getOutputPosition() == servoPosition.getOutputPosition()) {
                std::string errorMessage = fmt::format("Unable to insert the same output position twice: {}", servoPosition.getOutputPosition());
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

        // Transform each ServoPosition to a string and accumulate with a tab separator
        message = std::accumulate(servoPositions.begin(), servoPositions.end(), message,
                                  [](const std::string& accumulator, const creatures::ServoPosition& position) {
                                      return accumulator + '\t' + position.toString();
                                  });

        logger->trace("message is: {}", message);
        return message;
    }

    std::string SetServoPositions::toMessageWithChecksum() {

        u16 checksum = getChecksum();
        logger->trace("checksum is: {}", checksum);
        return fmt::format("{}\tCS {}", toMessage(), checksum);
    }

} // creatures