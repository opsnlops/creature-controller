
#pragma once

#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "logging/Logger.h"
#include "creature/Creature.h"


namespace creatures {

    /**
     * This class reads in a JSON file from the file system and returns a
     * configuration for the creature we're using.
     *
     * This used to be done using a hardcoded value in the Pi's EEPROM, but it's
     * a pain to tweak things when I'm having to do the build -> flash dance over
     * and over again.
     */
    class CreatureBuilder {

    public:
        CreatureBuilder(std::shared_ptr<Logger> logger, std::unique_ptr<std::istream> configFile);
        ~CreatureBuilder() = default;

        /**
         * Parses out the creature configuration from the JSON file
         *
         * @return a shared_ptr to our prize
         */
        std::shared_ptr<Creature> build();

        // Convert a file to an istream
        static std::unique_ptr<std::istream> fileToStream(std::shared_ptr<Logger> logger, std::string filename);

    private:
        std::unique_ptr<std::istream> configFile;
        std::vector<std::string> requiredTopLevelFields;
        std::vector<std::string> requiredServoFields;

        std::shared_ptr<Servo> createServo(const json& j);

        // Make sure the file is both readable and accessible
        static bool isFileAccessible(std::shared_ptr<Logger> logger, const std::string& filename);

        // Make sure a JSON field is present
        static void checkJsonField(const json& jsonObj, const std::string& fieldName);

        std::shared_ptr<Logger> logger;

    };

} // creatures
