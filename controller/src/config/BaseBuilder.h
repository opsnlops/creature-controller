
#pragma once

#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

#include "config/BuilderException.h"
#include "logging/Logger.h"
#include "util/Result.h"

namespace creatures ::config {

/**
 * This class is a base class for all builders. It provides some common functionality
 * that we use in all of our builders.
 */
class BaseBuilder {

  public:
    BaseBuilder(std::shared_ptr<Logger> logger, std::string fileName);
    ~BaseBuilder() = default;

    static Result<std::string> loadFile(std::shared_ptr<Logger> logger, std::string fileName);

  protected:
    std::string fileName;

    // Make sure the file is both readable and accessible
    static Result<bool> isFileAccessible(std::shared_ptr<Logger> logger, const std::string &filename);

    // Make sure a JSON field is present
    static Result<bool> checkJsonField(const json &jsonObj, const std::string &fieldName);

    // Safely extract a boolean value with proper error handling
    static Result<bool> getBooleanField(const json &jsonObj, const std::string &fieldName);

    std::shared_ptr<Logger> logger;
};

} // namespace creatures::config
