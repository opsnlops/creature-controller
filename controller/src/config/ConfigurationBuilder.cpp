
#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "config/BuilderException.h"
#include "config/Configuration.h"
#include "config/ConfigurationBuilder.h"
#include "config/UARTDevice.h"
#include "logging/Logger.h"
#include "util/Result.h"

namespace creatures ::config {

ConfigurationBuilder::ConfigurationBuilder(std::shared_ptr<Logger> logger, std::string configFileName)
    : BaseBuilder(logger, std::move(configFileName)) {

    // Define the required config file fields
    requiredTopLevelFields = {
        "useGPIO",
        "useRTPAudio",
        "audioDevice",
        "UARTs",
        "networkInterface",
        "universe",
        "powerDrawLimitWatts",
        "powerDrawWarningWatts",
        "powerDrawResponseSeconds",
        "temperatureLimitDegrees",
        "temperatureWarningDegrees",
        "temperatureLimitSeconds",
    };

    requiredUARTFields = {
        "enabled",
        "deviceNode",
        "module",
    };

    // Needed only if we're using the creature server. The controller can run
    // without it if needed, using only the e1.31 protocol.
    requiredServerFields = {
        "enabled",
        "address",
        "port",
    };
}

Result<std::shared_ptr<creatures::config::Configuration>> ConfigurationBuilder::build() {

    logger->info("about to try to parse the main config file");

    // Make sure the file is accessible
    auto fileResult = isFileAccessible(logger, fileName);
    if (!fileResult.isSuccess()) {
        return makeError(fmt::format("Unable to determine if {} is accessible", fileName));
    }

    if (!fileResult.getValue().value()) {
        return makeError(fmt::format("File {} is not accessible", fileName));
    }

    // Okay we have a valid-ish config! Let's start building the Configuration
    // object
    auto configFileResult = loadFile(logger, fileName);
    if (!configFileResult.isSuccess()) {
        return makeError(fmt::format("Unable to open {} for reading", fileName));
    }
    auto configFile = std::move(configFileResult.getValue().value());

    json j;
    try {
        j = json::parse(configFile);
        logger->debug("JSON file was valid JSON! Now let's see if it's got "
                      "what we need... 🤔");
    } catch (json::parse_error &e) {
        return makeError(fmt::format("JSON parse error: {}", e.what()));
    }

    if (!j.is_object()) {
        return makeError("JSON is not an object");
    }

    // Make sure the top level fields we need are present
    logger->debug("checking for required top level fields");
    for (const auto &fieldName : requiredTopLevelFields) {
        auto fieldResult = checkJsonField(j, fieldName);
        if (!fieldResult.isSuccess()) {
            return makeError(fmt::format("Missing required field: {}", fieldName));
        }
    }

    // Okay we have a valid-ish config! Let's start building the Configuration
    // object
    auto config = std::make_shared<creatures::config::Configuration>(logger);

    // Fill in the easy ones
    auto useGPIOResult = getBooleanField(j, "useGPIO");
    if (!useGPIOResult.isSuccess()) {
        return makeError(useGPIOResult.getError().value().getMessage());
    }
    config->setUseGPIO(useGPIOResult.getValue().value());

    config->setUniverse(j["universe"]);

    auto useRTPAudioResult = getBooleanField(j, "useRTPAudio");
    if (!useRTPAudioResult.isSuccess()) {
        return makeError(useRTPAudioResult.getError().value().getMessage());
    }
    config->setUseAudioSubsystem(useRTPAudioResult.getValue().value());

    config->setSoundDeviceNumber(j["audioDevice"]);
    config->setNetworkDeviceName(j["networkInterface"]);

    // Set watchdog configuration
    config->setPowerDrawLimitWatts(j["powerDrawLimitWatts"]);
    config->setPowerDrawWarningWatts(j["powerDrawWarningWatts"]);
    config->setPowerDrawResponseSeconds(j["powerDrawResponseSeconds"]);
    config->setTemperatureLimitDegrees(j["temperatureLimitDegrees"]);
    config->setTemperatureWarningDegrees(j["temperatureWarningDegrees"]);
    config->setTemperatureLimitSeconds(j["temperatureLimitSeconds"]);

    // Log that we've gotten this far
    logger->info("successfully parsed the main config file! useGPIO: {}, "
                 "networkInterface: {}, universe: {}",
                 config->getUseGPIO(), config->getNetworkDeviceName(), config->getUniverse());

    // Now go look up the network information
    config->resolveNetworkInterfaceDetails();
    logger->debug("successfully resolved network interface {}. IP Address: {}, index: {}",
                  config->getNetworkDeviceName(), config->getNetworkDeviceIPAddress(), config->getNetworkDeviceIndex());

    // Now let's handle the UARTs
    for (auto &uart : j["UARTs"]) {

        // Validate the fields in this object
        for (const auto &fieldName : requiredUARTFields) {
            checkJsonField(uart, fieldName);
        }

        std::string deviceNode = uart["deviceNode"];
        auto enabledResult = getBooleanField(uart, "enabled");
        if (!enabledResult.isSuccess()) {
            return makeError(
                fmt::format("Error in UART configuration: {}", enabledResult.getError().value().getMessage()));
        }
        bool enabled = enabledResult.getValue().value();
        std::string moduleAsString = uart["module"];

        logger->debug("working on UART: {}", deviceNode);

        // Is this a valid module ID?
        creatures::config::UARTDevice::module_name moduleName =
            creatures::config::UARTDevice::stringToModuleName(moduleAsString);
        switch (moduleName) {

        // Do this in its own context since there's a var being created
        case creatures::config::UARTDevice::invalid_module:
            return makeError(fmt::format("invalid module ID: {}", moduleAsString));
        default:
            logger->debug("module ID is valid: {}", moduleAsString);
        }

        creatures::config::UARTDevice uartDevice(logger);
        uartDevice.setDeviceNode(deviceNode);
        uartDevice.setModule(moduleName);
        uartDevice.setEnabled(enabled);
        config->addUARTDevice(uartDevice);

        logger->debug("added UART to the config: {}", deviceNode);
    }
    logger->debug("done processing uarts");

    // Determine if the server is enabled
    if (j.contains(serverNode)) {

        logger->debug("found the server node, attempting to parse the server values");

        // Validate the fields in this object
        for (const auto &fieldName : requiredServerFields) {
            checkJsonField(j[serverNode], fieldName);
        }

        auto enabledResult = getBooleanField(j[serverNode], "enabled");
        if (!enabledResult.isSuccess()) {
            return makeError(
                fmt::format("Error in server configuration: {}", enabledResult.getError().value().getMessage()));
        }
        bool enabled = enabledResult.getValue().value();
        std::string address = j[serverNode]["address"];
        u16 port = j[serverNode]["port"];

        logger->info("server is enabled: {}, address: {}, port: {}", enabled, address, port);

        config->setUseServer(enabled);
        config->setServerAddress(address);
        config->setServerPort(port);
    } else {
        logger->debug("server node ({}) not found, assuming server is disabled", serverNode);
        config->setUseServer(false);
    }

    logger->info("done parsing the main config file");
    return Result<std::shared_ptr<creatures::config::Configuration>>{config};
}

/**
 * Quick helper function to make error messages consistently
 * @param errorMessage The error to create
 * @return A Result object with the error message
 */
Result<std::shared_ptr<creatures::config::Configuration>>
ConfigurationBuilder::makeError(const std::string &errorMessage) const {
    logger->error(errorMessage);
    return Result<std::shared_ptr<creatures::config::Configuration>>{
        ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
}

} // namespace creatures::config
