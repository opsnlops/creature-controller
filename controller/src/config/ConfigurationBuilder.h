
#pragma once

#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "config/BaseBuilder.h"
#include "config/Configuration.h"
#include "config/UARTDevice.h"
#include "logging/Logger.h"


using namespace creatures;
using namespace creatures::config;

namespace creatures :: config {

    /**
     * This file loads in our configuration from a JSON file and returns a `Configuration` object
     */
    class ConfigurationBuilder : public BaseBuilder {

    public:
        ConfigurationBuilder(std::shared_ptr<Logger> logger, std::unique_ptr<std::istream> configFile);
        ~ConfigurationBuilder() = default;

        /**
         * Parses out the creature configuration from the JSON file
         *
         * @return a shared_ptr to our prize
         */
        std::shared_ptr<Configuration> build();

    private:
        std::vector<std::string> requiredTopLevelFields;
        std::vector<std::string> requiredUARTFields;

    };

} // creatures :: config
