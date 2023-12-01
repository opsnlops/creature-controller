//
// Created by April White on 11/30/23.
//

#include <string>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "creature_builder.h"
#include "CreatureBuilderException.h"

#include "namespace-stuffs.h"

namespace creatures {

    CreatureBuilder::CreatureBuilder(std::string filename) {

        // Make sure the file name is valid
        if(filename.length() == 0) {
            error("no file name provided");
            throw CreatureBuilderException("no file name provided");
        }

        // Make sure we can both access and read the file
        if(!isFileAccessible(filename)) {
            error("file {} is not accessible", filename);
            throw CreatureBuilderException(fmt::format("file {} is not accessible", filename));
        }

        this->configFile = filename;
        info("set file name to {}",this->configFile);
    }


    bool CreatureBuilder::parseConfig() {

        info("about to try to parse the config file");

        std::ifstream f(this->configFile);
        json data = json::parse(f);
        debug("file was parsed!");

        info("creature name is {} (version {})", data["name"], data["version"]);




        return true;

    }


    /**
     * Ensures that a give file is both readable and accessible
     *
     * @param filename the file to check
     * @return true if the file is both readable and accessible
     */
    bool CreatureBuilder::isFileAccessible(const std::string& filename) {

        debug("making sure that {} is accessible and readable", filename);

        // Check if the file exists
        if (!std::filesystem::exists(filename)) {
            return false;
        }
        debug("file exists");

        // Try to open the file for reading
        std::ifstream file(filename);
        return file.good();
    }

} // creatures