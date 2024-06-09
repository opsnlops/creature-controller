
#include <numeric>
#include <string>
#include <vector>

#include "controller/Controller.h"
#include "controller/commands/tokens/ServoConfig.h"
#include "config/UARTDevice.h"
#include "logging/Logger.h"
#include "util/Result.h"


#include "ServoModuleConfiguration.h"

namespace creatures::commands {

    ServoModuleConfiguration::ServoModuleConfiguration(std::shared_ptr<Logger> logger) :
                                                 logger(logger) { // NOLINT(*-pass-by-value)
        this->servoConfigurations = std::vector<ServoConfig>();
    }

    Result<bool> ServoModuleConfiguration::getServoConfigurations(const std::shared_ptr<Controller>& controller,
                                                                 creatures::config::UARTDevice::module_name module) {

        auto configResult = controller->getServoConfigs(module);
        if(!configResult.isSuccess()) {
            auto errorMessage = fmt::format("Failed to get servo configurations: {}", configResult.getError()->getMessage());
            logger->warn(errorMessage);
            return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
        }

        auto configs = configResult.getValue().value();
        logger->debug("got the servo configurations for module {} ({} total)",
                      UARTDevice::moduleNameToString(module),
                      configs.size());

        // Now add them to the vector
        for (const auto& config : configs) {
            this->addServoConfig(config);
        }

        return Result<bool>{true};
    }

    Result<bool> ServoModuleConfiguration::addServoConfig(const ServoConfig &servoConfig) {

        // Make sure we're not putting the same outputPosition in twice
        for (const auto& existingConfigurations : servoConfigurations) {
            if (existingConfigurations.getOutputHeader() == servoConfig.getOutputHeader()) {
                std::string errorMessage = fmt::format("Unable to insert the same output position twice: {}", servoConfig.getOutputHeader());
                logger->error(errorMessage);
                return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
            }
        }

        this->servoConfigurations.push_back(servoConfig);
        logger->trace("Add config for servo: {}", servoConfig.toString());

        return Result<bool>{true};
    }

    std::string ServoModuleConfiguration::toMessage() {

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