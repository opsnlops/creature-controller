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
#include "creature/creature.h"
#include "creature/parrot.h"

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

        // Define the required config file fields
        requiredTopLevelFields = {
                "type", "name", "version", "starting_dmx_channel", "motors",
                "head_offset_max", "servo_frequency", "position_min", "position_max",
                "description",
        };

        requiredServoFields = {
                "type", "id", "name", "output_pin", "min_pulse_us", "max_pulse_us",
                "smoothing_value", "inverted", "default_position"
        };



        this->configFile = filename;
        info("set file name to {}",this->configFile);
    }


    std::shared_ptr<Creature> CreatureBuilder::build() {

        info("about to try to parse the config file");

        std::ifstream f(this->configFile);
        json j = json::parse(f);
        debug("file was parsed!");


        // Make sure the top level fields we need are present
        for (const auto& fieldName : requiredTopLevelFields) {
            checkJsonField(j, fieldName);
        }

        // Validate the creature type
        Creature::creature_type type = Creature::stringToCreatureType(j["type"]);
        if(type == Creature::invalid_creature) {
            error("invalid creature type: {}", j["type"]);
            throw CreatureBuilderException(fmt::format("invalid creature type: {}", j["type"]));
        }

        std::shared_ptr<Creature> creature;

        switch(type) {
            case Creature::parrot:
                creature = std::make_shared<Parrot>();
                break;
            default:
                error("unimplemented creature type: {}", j["type"]);
                std::exit(1);
                break;
        }

        creature->setName(j["name"]);
        creature->setVersion(j["version"]);
        creature->setDescription(j["description"]);
        creature->setStartingDmxChannel(j["starting_dmx_channel"]);
        creature->setPositionMin(j["position_min"]);
        creature->setPositionMax(j["position_max"]);
        creature->setHeadOffsetMax(j["head_offset_max"]);
        creature->setServoFrequency(j["servo_frequency"]);
        creature->setType(type);

        // Log that we've gotten this far
        info("creature name is {} (version {}), at channel offset {}",
             creature->getName(), creature->getVersion(), creature->getStartingDmxChannel());

        for(auto& motor : j["motors"]) {

            // Validate the fields in this object
            for (const auto& fieldName : requiredServoFields) {
                checkJsonField(motor, fieldName);
            }
            debug("looking at motor {}", motor["id"]);

            // Make sure we have a valid type for this motor
            Creature::motor_type motorType = Creature::stringToMotorType(motor["type"]);
            switch(motorType) {

                // Do this in it's own context since there's a var being created
                case Creature::servo: {
                    std::shared_ptr<Servo> servo = createServo(motor);
                    debug("adding servo {}", servo->getId());
                    creature->addServo(servo->getId(), servo);
                    break;
                }
                default:
                    error("invalid motor type: {}", motor["type"]);
                    throw CreatureBuilderException(fmt::format("invalid motor type: {}", motor["type"]));

            }

        }
        debug("done processing motors");

        // TODO: Handle the inputs

        return creature;

    }


    /**
     * Builds a Servo object from a JSON object. Assumes that the
     * JSON object has already been validated.
     *
     * @param j
     * @return a `std::shared_ptr<Servo>` to the new servo
     */
    std::shared_ptr<Servo> CreatureBuilder::createServo(const json& j) {

        Creature::motor_type type = Creature::stringToMotorType(j["type"]);
        if(type == Creature::invalid_motor) {
            throw CreatureBuilderException(fmt::format("invalid motor type: {}", j["type"]));
        }

        std::string id = j["id"];
        std::string name = j["name"];
        u8 output_pin = j["output_pin"];
        u16 min_pulse_us = j["min_pulse_us"];
        u16 max_pulse_us = j["max_pulse_us"];
        float smoothing_value = j["smoothing_value"];
        bool inverted = j["inverted"];
        std::string parseDefault = j["default_position"];

        // Figure out what the default position should be
        u16 default_position = 0;
        Creature::default_position_type requestedDefault = Creature::stringToDefaultPositionType(parseDefault);

        switch(requestedDefault) {
            case Creature::center:
                default_position = min_pulse_us + ((max_pulse_us - min_pulse_us) / 2);
                break;
            case Creature::min:
                default_position = min_pulse_us;
                break;
            case Creature::max:
                default_position = max_pulse_us;
                break;
            case Creature::invalid_position:
                throw CreatureBuilderException(fmt::format("invalid default position: {}", parseDefault));
                break;
        }

        // Create the servo
        return std::make_shared<Servo>(id, output_pin, name, min_pulse_us, max_pulse_us,
                                       smoothing_value, inverted, default_position);

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

    /**
     * Checks to make sure that a field is defined in a JSON file. Will throw
     * a `CreatureBuilderException` if it's not there.
     *
     * @param jsonObj the json object to check
     * @param fieldName the field name we're looking for
     */
    void CreatureBuilder::checkJsonField(const nlohmann::json& jsonObj, const std::string& fieldName) {
        if (!jsonObj.contains(fieldName)) {
            throw CreatureBuilderException("Missing required field: " + fieldName);
        }
    }

} // creatures