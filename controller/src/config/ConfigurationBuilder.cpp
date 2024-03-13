
#include <fstream>
#include <string>
#include <sstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "config/BuilderException.h"
#include "config/Configuration.h"
#include "config/ConfigurationBuilder.h"
#include "config/UARTDevice.h"
#include "logging/Logger.h"


namespace creatures :: config {

    ConfigurationBuilder::ConfigurationBuilder(std::shared_ptr<Logger> logger,
                                     std::unique_ptr<std::istream> configFile) :
                                     BaseBuilder(logger, std::move(configFile)) {

        // Define the required config file fields
        requiredTopLevelFields = {
                "useGPIO", "UARTs", "ipAddress", "universe",
        };

        requiredUARTFields = {
                "enabled", "deviceNode", "module",
        };
    }


    std::shared_ptr<creatures::config::Configuration> ConfigurationBuilder::build() {

        logger->info("about to try to parse the main config file");

        std::string content((std::istreambuf_iterator<char>(*configFile)), std::istreambuf_iterator<char>());
        logger->debug("JSON file contents: {}", content);
        configFile->seekg(0);

        json j;
        try {
            j = json::parse(content);
            logger->debug("file was parsed!");
        } catch (json::parse_error& e) {
            logger->error("JSON parse error: {}", e.what());
            throw BuilderException(fmt::format("JSON parse error: {}", e.what()));
        }

        if (!j.is_object()) {
            logger->error("JSON is not an object");
            throw BuilderException("JSON is not an object");
        }

        // Make sure the top level fields we need are present
        for (const auto& fieldName : requiredTopLevelFields) {
            checkJsonField(j, fieldName);
        }

        // Okay we have a valid-ish config! Let's start building the Configuration object
        auto config = std::make_shared<creatures::config::Configuration>(logger);

        // Fill in the easy ones
        config->setUseGPIO(j["useGPIO"]);
        config->setNetworkDeviceIPAddress(j["ipAddress"]);
        config->setUniverse(j["universe"]);


        // Log that we've gotten this far
        logger->info("successfully parsed the main config file! useGPIO: {}, ipAddress: {}, universe: {}",
                     config->getUseGPIO(), config->getNetworkDeviceIPAddress(), config->getUniverse());

        // Now let's handle the UARTs
        for(auto& uart : j["UARTs"]) {

            // Validate the fields in this object
            for (const auto& fieldName : requiredUARTFields) {
                checkJsonField(uart, fieldName);
            }

            std::string deviceNode = uart["deviceNode"];
            bool enabled = uart["enabled"];
            std::string moduleAsString = uart["module"];

            logger->debug("working on UART: {}", deviceNode);

            // Is this a valid module ID?
            creatures::config::UARTDevice::module_name moduleName = creatures::config::UARTDevice::stringToModuleName(moduleAsString);
            switch(moduleName) {

                // Do this in its own context since there's a var being created
                case creatures::config::UARTDevice::invalid_module:
                    logger->error("invalid module ID: {}", moduleAsString);
                    throw BuilderException(fmt::format("invalid module ID", moduleAsString));

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

        logger->info("done parsing the main config file");
        return config;

    }

} // creatures :: config
