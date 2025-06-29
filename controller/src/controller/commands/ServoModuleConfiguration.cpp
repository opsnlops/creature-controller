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
                                                 logger(logger) {
        this->servoConfigurations = std::vector<ServoConfig>();
        logger->debug("ServoModuleConfiguration created - ready to gather servo configs! 🐰");
    }

    Result<bool> ServoModuleConfiguration::getServoConfigurations(const std::shared_ptr<Controller>& controller,
                                                                 creatures::config::UARTDevice::module_name module) {

        if (!controller) {
            auto errorMessage = "Controller is null in getServoConfigurations";
            logger->error(errorMessage);
            return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
        }

        auto configResult = controller->getServoConfigs(module);
        if(!configResult.isSuccess()) {
            auto errorMessage = fmt::format("Failed to get servo configurations: {}",
                                           configResult.getError()->getMessage());
            logger->error(errorMessage);
            return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
        }

        auto configs = configResult.getValue().value();
        logger->debug("got {} servo configurations for module {} - time to hop them into place! 🐰",
                      configs.size(),
                      creatures::config::UARTDevice::moduleNameToString(module));

        // Clear any existing configurations before adding new ones
        servoConfigurations.clear();

        // Add each configuration
        for (const auto& config : configs) {
            auto addResult = this->addServoConfig(config);
            if (!addResult.isSuccess()) {
                // If any config fails to add, return the error
                return addResult;
            }
        }

        return Result<bool>{true};
    }

    Result<bool> ServoModuleConfiguration::addServoConfig(const ServoConfig &servoConfig) {

        // Check for duplicate output positions
        for (const auto& existingConfig : servoConfigurations) {
            if (existingConfig.getOutputHeader() == servoConfig.getOutputHeader()) {
                std::string errorMessage = fmt::format("Duplicate output position {}: servo configurations must be unique",
                                                       servoConfig.getOutputHeader());
                logger->error(errorMessage);
                return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
            }
        }

        this->servoConfigurations.push_back(servoConfig);
        logger->trace("Added servo config: {}", servoConfig.toString());

        return Result<bool>{true};
    }

    std::string ServoModuleConfiguration::toMessage() {

        // Make sure we have configurations to send
        if (servoConfigurations.empty()) {
            logger->warn("No servo configurations to send - did you forget to call getServoConfigurations()?");
            return "CONFIG"; // Send empty config message
        }

        // Start the message with the 'CONFIG' command prefix
        std::string message = "CONFIG";

        // Add each servo configuration separated by tabs
        for (const auto& config : servoConfigurations) {
            message += '\t' + config.toString();
        }

        logger->info("servo config message ready to hop over to firmware: {}", message);
        return message;
    }

} // creatures::commands