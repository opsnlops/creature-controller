//
// Created by April White on 11/30/23.
//

#ifndef CREATURE_CONTROLLER_CREATURE_BUILDER_H
#define CREATURE_CONTROLLER_CREATURE_BUILDER_H

#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "creature/creature.h"

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
        CreatureBuilder(std::string filename);
        ~CreatureBuilder() = default;

        bool parseConfig();

        Creature getCreature();

    private:
        std::string configFile;
        std::vector<std::string> requiredFields;

        // Make sure the file is both readable and accessible
        static bool isFileAccessible(const std::string& filename);

        // Make sure a JSON field is present
        static void checkJsonField(const json& jsonObj, const std::string& fieldName);
    };

} // creatures

#endif //CREATURE_CONTROLLER_CREATURE_BUILDER_H
