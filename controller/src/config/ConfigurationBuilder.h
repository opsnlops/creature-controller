
#pragma once

#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "config/BaseBuilder.h"
#include "config/CommandLine.h"
#include "config/UARTDevice.h"
#include "logging/Logger.h"
#include "util/Result.h"

using namespace creatures;
using namespace creatures::config;

namespace creatures ::config {

class Configuration; // Forward declaration

/**
 * This file loads in our configuration from a JSON file and returns a `Configuration` object
 */
class ConfigurationBuilder : public BaseBuilder {

    friend class Configuration;
    friend class CommandLine;

  public:
    /**
     * Parses out the creature configuration from the JSON file
     *
     * @return a shared_ptr to our prize
     */
    Result<std::shared_ptr<Configuration>> build();

    ConfigurationBuilder(std::shared_ptr<Logger> logger, std::string configFileName);
    ~ConfigurationBuilder() = default;

  private:
    std::vector<std::string> requiredTopLevelFields;
    std::vector<std::string> requiredUARTFields;

    const std::string serverNode = "creatureServer";
    std::vector<std::string> requiredServerFields;

    Result<std::shared_ptr<Configuration>> makeError(const std::string &errorMessage) const;
};

} // namespace creatures::config
