
#pragma once

#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "config/BaseBuilder.h"
#include "creature/Creature.h"
#include "logging/Logger.h"
#include "util/Result.h"



namespace creatures :: config {

    /**
     * This class reads in a JSON file from the file system and returns a
     * configuration for the creature we're using.
     *
     * This used to be done using a hardcoded value in the Pi's EEPROM, but it's
     * a pain to tweak things when I'm having to do the build -> flash dance over
     * and over again.
     */
    class CreatureBuilder : public BaseBuilder {

    public:
        CreatureBuilder(std::shared_ptr<Logger> logger, std::string configFile);
        ~CreatureBuilder() = default;

        /**
         * Parses out the creature configuration from the JSON file
         *
         * @return a shared_ptr to our prize
         */
        Result<std::shared_ptr<creatures::creature::Creature>> build();

    private:
        std::vector<std::string> requiredTopLevelFields;
        std::vector<std::string> requiredServoFields;
        std::vector<std::string> requiredInputFields;

        std::shared_ptr<Servo> createServo(const json& j, u16 servo_frequency);

    };

} // creatures :: config
