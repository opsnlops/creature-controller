

#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "CreatureBuilder.h"
#include "CreatureBuilderException.h"

#include "logging/Logger.h"
#include "creature/Creature.h"
#include "controller/Input.h"
#include "creature/Parrot.h"


namespace creatures :: config {

    CreatureBuilder::CreatureBuilder(std::shared_ptr<Logger> logger,
                                     std::unique_ptr<std::istream> configFile) :
                                     BaseBuilder(logger, std::move(configFile)) {

        // Define the required config file fields
        requiredTopLevelFields = {
                "type", "name", "version", "channel_offset", "universe", "motors",
                "head_offset_max", "servo_frequency", "position_min", "position_max",
                "description",
        };

        requiredServoFields = {
                "type", "id", "name", "output_location", "min_pulse_us", "max_pulse_us",
                "smoothing_value", "inverted", "default_position"
        };

        requiredInputFields = {
                "name", "slot", "width"
        };

    }


    std::shared_ptr<creatures::creature::Creature> CreatureBuilder::build() {

        logger->info("about to try to parse the creature config file");

        std::string content((std::istreambuf_iterator<char>(*configFile)), std::istreambuf_iterator<char>());
        logger->debug("JSON file contents: {}", content);
        configFile->seekg(0);

        json j;
        try {
            j = json::parse(content);
            logger->debug("file was parsed!");
        } catch (json::parse_error& e) {
            logger->error("JSON parse error: {}", e.what());
            throw CreatureBuilderException(fmt::format("JSON parse error: {}", e.what()));
        }

        if (!j.is_object()) {
            logger->error("JSON is not an object");
            throw CreatureBuilderException("JSON is not an object");
        }

        // Make sure the top level fields we need are present
        for (const auto& fieldName : requiredTopLevelFields) {
            checkJsonField(j, fieldName);
        }

        // Validate the creature type
        creatures::creature::Creature::creature_type type = creatures::creature::Creature::stringToCreatureType(j["type"]);
        std::string string_type = j["type"]; // Render the type to a string so fmt can print it
        if(type == creatures::creature::Creature::invalid_creature) {
            logger->error("invalid creature type: {}", string_type);
            throw CreatureBuilderException(fmt::format("invalid creature type: {}", string_type));
        }

        std::shared_ptr<creatures::creature::Creature> creature;

        switch(type) {
            case creatures::creature::Creature::parrot:
                creature = std::make_shared<Parrot>(logger);
                break;
            default:
                logger->error("unimplemented creature type: {}", string_type);
                std::exit(1);
                break;
        }

        // The servo frequency is shared. There can only be one per creature, so it's
        // set in the creature tree, but it needs to be passed into the servos. It
        // needs to know this to do math on position updates.
        u16 servo_frequency = j["servo_frequency"];

        creature->setName(j["name"]);
        creature->setVersion(j["version"]);
        creature->setDescription(j["description"]);
        creature->setChannelOffset(j["channel_offset"]);
        creature->setUniverse(j["universe"]);
        creature->setPositionMin(j["position_min"]);
        creature->setPositionMax(j["position_max"]);
        creature->setHeadOffsetMax(j["head_offset_max"]);
        creature->setServoUpdateFrequencyHz(servo_frequency);
        creature->setType(type);

        // Log that we've gotten this far
        logger->info("creature name is {} (version {}), at channel offset {}",
             creature->getName(), creature->getVersion(), creature->getChannelOffset());

        for(auto& motor : j["motors"]) {

            std::string id_string = motor["id"];
            std::string type_string = motor["type"];

            // Validate the fields in this object
            for (const auto& fieldName : requiredServoFields) {
                checkJsonField(motor, fieldName);
            }
            logger->debug("looking at motor {}", id_string);

            // Make sure we have a valid type for this motor
            creatures::creature::Creature::motor_type motorType = creatures::creature::Creature::stringToMotorType(motor["type"]);
            switch(motorType) {

                // Do this in its own context since there's a var being created
                case creatures::creature::Creature::servo: {
                    std::shared_ptr<Servo> servo = createServo(motor, servo_frequency);
                    logger->debug("adding servo {}", servo->getId());
                    creature->addServo(servo->getId(), servo);
                    break;
                }
                default:
                    logger->error("invalid motor type: {}", type_string);
                    throw CreatureBuilderException(fmt::format("invalid motor type: {}", type_string));

            }

        }
        logger->debug("done processing motors");

        // Handle the inputs
        for(auto& input : j["inputs"]) {

            // Validate the fields in this object
            for (const auto& fieldName : requiredInputFields) {
                checkJsonField(input, fieldName);
            }

            std::string inputName = input["name"];
            u16 inputSlot = input["slot"];
            u8 inputWidth = input["width"];

            // Make sure the slot is in the correct range for DMX
            if(inputSlot > 512) {
                throw CreatureBuilderException(fmt::format("input slot {} is out of range", inputSlot));
            }

            creature->addInput(creatures::Input(inputName, inputSlot, inputWidth, 0UL));
            logger->debug("added input {} at slot {}", inputName, inputSlot);

        }

        return creature;

    }


    /**
     * Builds a Servo object from a JSON object. Assumes that the
     * JSON object has already been validated.
     *
     * @param j
     * @return a `std::shared_ptr<Servo>` to the new servo
     */
    std::shared_ptr<Servo> CreatureBuilder::createServo(const json& j, u16 servo_frequency) {

        creatures::creature::Creature::motor_type type = creatures::creature::Creature::stringToMotorType(j["type"]);
        if(type == creatures::creature::Creature::invalid_motor) {
            std::string type_string = j["type"];
            throw CreatureBuilderException(fmt::format("invalid motor type: {}", type_string));
        }

        std::string id = j["id"];
        std::string name = j["name"];
        std::string output_location = j["output_location"];
        u16 min_pulse_us = j["min_pulse_us"];
        u16 max_pulse_us = j["max_pulse_us"];
        float smoothing_value = j["smoothing_value"];
        bool inverted = j["inverted"];
        std::string parseDefault = j["default_position"];

        // Figure out what the default position should be
        u16 default_position = 0;
        creatures::creature::Creature::default_position_type requestedDefault = creatures::creature::Creature::stringToDefaultPositionType(parseDefault);

        switch(requestedDefault) {
            case creatures::creature::Creature::center:
                default_position = min_pulse_us + ((max_pulse_us - min_pulse_us) / 2);
                break;
            case creatures::creature::Creature::min:
                default_position = min_pulse_us;
                break;
            case creatures::creature::Creature::max:
                default_position = max_pulse_us;
                break;
            case creatures::creature::Creature::invalid_position:
                throw CreatureBuilderException(fmt::format("invalid default position: {}", parseDefault));
                break;
        }

        // Create the servo
        return std::make_shared<Servo>(logger, id, output_location, name, min_pulse_us, max_pulse_us,
                                       smoothing_value, inverted, servo_frequency, default_position);

    }

} // creatures :: config
