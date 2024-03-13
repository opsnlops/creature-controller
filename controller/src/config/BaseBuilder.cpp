
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


namespace creatures :: config {

    BaseBuilder::BaseBuilder(std::shared_ptr<Logger> logger,
                             std::string fileName) : logger(logger), fileName(std::move(fileName)) {}

    /**
     * Ensures that a give file is both readable and accessible
     *
     * @param filename the file to check
     * @return true if the file is both readable and accessible
     */
    bool BaseBuilder::isFileAccessible(std::shared_ptr<Logger> logger, const std::string& filename) {

        logger->debug("making sure that {} is accessible and readable", filename);

        // Check if the file exists
        if (!std::filesystem::exists(filename)) {
            return false;
        }
        logger->debug("file exists");

        // Try to open the file for reading
        std::ifstream file(filename);
        return file.good();
    }

    /**
     * Checks to make sure that a field is defined in a JSON file. Will throw
     * a `CreatureBuilderException` if it's not there.
     *
     * @param jsonObj the json object to check
     * @param fieldName the field name we're looking for
     */
    void BaseBuilder::checkJsonField(const nlohmann::json& jsonObj, const std::string& fieldName) {
        if (!jsonObj.contains(fieldName)) {
            throw BuilderException("Missing required field: " + fieldName);
        }
    }

    std::unique_ptr<std::istream> BaseBuilder::fileToStream(std::shared_ptr<Logger> logger, std::string filename) {

        logger->debug("turning {} into an istream", filename);

        // Make sure the file name is valid
        if(filename.empty()) {
            logger->error("no file name provided");
            throw BuilderException("no file name provided");
        }

        // Make sure we can both access and read the file
        if(!isFileAccessible(logger, filename)) {
            logger->error("file {} is not accessible", filename);
            throw BuilderException(fmt::format("file {} is not accessible", filename));
        }

        // Open the file and read its contents into a stringstream
        std::ifstream fileStream(filename);
        if (!fileStream) {
            logger->error("Failed to open file {}", filename);
            throw BuilderException(fmt::format("Failed to open file {}", filename));
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

        return stringStream;

    }

} // creatures :: config
