
#include <numeric>
#include <string>
#include <vector>

#include "controller-config.h"

#include "device/Servo.h"
#include "logging/Logger.h"
#include "controller/commands/ICommand.h"
#include "controller/commands/tokens/ServoConfig.h"

#include "controller/commands/CommandException.h"

#include "CreatureConfiguration.h"

namespace creatures::commands {

    CreatureConfiguration::CreatureConfiguration(std::shared_ptr<Logger> logger) :
                                                 logger(logger) { // NOLINT(*-pass-by-value)
        this->servoConfigurations = std::vector<ServoConfig>();
    }

    void CreatureConfiguration::getServoConfigurations(std::shared_ptr<creatures::creature::Creature> creature) {

        auto configs = creature->getServoConfigs();
        logger->debug("got the servo configurations ({} total)", configs.size());

        // Now add them to the vector
        for (const auto& config : configs) {
            this->addServoConfig(config);
        }
    }

    void CreatureConfiguration::addServoConfig(const ServoConfig &servoConfig) {

        // Make sure we're not putting the same outputPosition in twice
        for (const auto& existingConfigurations : servoConfigurations) {
            if (existingConfigurations.getOutputPosition() == servoConfig.getOutputPosition()) {
                std::string errorMessage = fmt::format("Unable to insert the same output position twice: {}", servoConfig.getOutputPosition());
                logger->error(errorMessage);
                throw CommandException(errorMessage);
            }
        }

        this->servoConfigurations.push_back(servoConfig);
        logger->trace("Add config for servo: {}", servoConfig.toString());
    }

    std::string CreatureConfiguration::toMessage() {

        // Yell if we're doing this on a blank set of positions
        if (servoConfigurations.empty()) {
            logger->warn("attempted to call toMessage() on an empty set of servo positions! (did you forget to call getServoConfigurations?)");
            return "";
        }

        // Start the message with the 'CONFIG' command prefix
        std::string message = "CONFIG";

        // Go through each ServoConfig and accumulate with a tab separator
        message = std::accumulate(servoConfigurations.begin(), servoConfigurations.end(), message,
                                  [](const std::string& accumulator, const creatures::ServoConfig& config) {
                                      return accumulator + '\t' + config.toString();
                                  });

        logger->info("config message to send is: {}", message);
        return message;
    }

} // creatures::commands