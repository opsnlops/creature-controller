
#pragma once

#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "config/BuilderException.h"
#include "logging/Logger.h"

namespace creatures :: config {

    /**
     * This class is a base class for all builders. It provides some common functionality
     * that we use in all of our builders.
     */
    class BaseBuilder {

    public:
        BaseBuilder(std::shared_ptr<Logger> logger, std::unique_ptr<std::istream> configFile);

        // Convert a file to an istream
        static std::unique_ptr<std::istream> fileToStream(std::shared_ptr<Logger> logger, std::string filename);

    protected:
        std::unique_ptr<std::istream> configFile;

        // Make sure the file is both readable and accessible
        static bool isFileAccessible(std::shared_ptr<Logger> logger, const std::string& filename);

        // Make sure a JSON field is present
        static void checkJsonField(const json& jsonObj, const std::string& fieldName);

        std::shared_ptr<Logger> logger;

    };

} // creatures :: config
