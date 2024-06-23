
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "config/BaseBuilder.h"
#include "config/BuilderException.h"
#include "logging/Logger.h"
#include "util/Result.h"


namespace creatures :: config {

    BaseBuilder::BaseBuilder(std::shared_ptr<Logger> logger,
                             std::string fileName) : fileName(std::move(fileName)), logger(logger) {}

    /**
     * Ensures that a give file is both readable and accessible
     *
     * @param filename the file to check
     * @return true if the file is both readable and accessible
     */
    Result<bool> BaseBuilder::isFileAccessible(std::shared_ptr<Logger> logger, const std::string& filename) {

        logger->debug("making sure that {} is accessible and readable", filename);

        // Check if the file exists
        if (!std::filesystem::exists(filename)) {
            auto message = fmt::format("File {} does not exist", filename);
            return Result<bool>{ControllerError(ControllerError::InvalidData, message)};
        }
        logger->debug("file exists");

        // Try to open the file for reading
        std::ifstream file(filename);
        return Result<bool>(file.good());
    }

    /**
     * Checks to make sure that a field is defined in a JSON file. Will throw
     * a `CreatureBuilderException` if it's not there.
     *
     * @param jsonObj the json object to check
     * @param fieldName the field name we're looking for
     */
    Result<bool> BaseBuilder::checkJsonField(const nlohmann::json& jsonObj, const std::string& fieldName) {
        try {
            // Make sure it exists
            if (!jsonObj.contains(fieldName)) {
                return Result<bool>(ControllerError(ControllerError::InvalidData, "Missing required field: " + fieldName));
            }

        } catch (nlohmann::json::out_of_range& e) {
            auto errorMessage = fmt::format("Error checking field {}: {}", fieldName, e.what());
            return Result<bool>(ControllerError(ControllerError::InternalError, errorMessage));
        }
        return Result<bool>(true);
    }

    Result<std::string> BaseBuilder::loadFile(std::shared_ptr<Logger> logger, std::string filename) {

        logger->debug("turning {} into an istream", filename);

        // Make sure the file name is valid
        if(filename.empty()) {
            logger->error("no file name provided");
            return Result<std::string>(ControllerError(ControllerError::InvalidConfiguration, "no file name provided"));
        }

        // Make sure we can both access and read the file
        auto fileAccessibleResult = isFileAccessible(logger, filename);
        if(!fileAccessibleResult.isSuccess()) {
            auto errorMessage = fmt::format("Unable to determine if {} is accessible: {}", filename, fileAccessibleResult.getError().value().getMessage());
            logger->error(errorMessage);
            return Result<std::string>{ControllerError(ControllerError::InternalError, errorMessage)};
        }
        bool fileAccessible = fileAccessibleResult.getValue().value();
        if(!fileAccessible) {
            auto errorMessage = fmt::format("File {} is not accessible", filename);
            logger->error(errorMessage);
            return Result<std::string>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
        }

        // Open the file and read its contents into a stringstream
        std::ifstream fileStream(filename);
        if (!fileStream) {
            auto errorMessage = fmt::format("Failed to open file {}", filename);
            logger->error(errorMessage);
            return Result<std::string>{ControllerError(ControllerError::InternalError, errorMessage)};
        }

        auto stringStream = std::make_unique<std::stringstream>();
        *stringStream << fileStream.rdbuf();

        // Reset the stream position to the beginning
        stringStream->seekg(0);

        // You might want to log the size of the stream content, if needed
        stringStream->seekg(0, std::ios::end);
        size_t size = stringStream->tellg();
        stringStream->seekg(0);
        logger->debug("Opened file {} with size {} bytes", filename, size);

        // Load the contents of the file into a string
        std::string content((std::istreambuf_iterator<char>(*stringStream)), std::istreambuf_iterator<char>());
        logger->debug("loaded the contents of {} into a string", filename);

        return Result<std::string>(content);

    }

} // creatures :: config
