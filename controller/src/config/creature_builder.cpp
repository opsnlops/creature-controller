

#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "creature_builder.h"
#include "CreatureBuilderException.h"

#include "logging/Logger.h"
#include "creature/creature.h"
#include "creature/parrot.h"


namespace creatures {

    CreatureBuilder::CreatureBuilder(std::shared_ptr<Logger> logger,
                                     std::unique_ptr<std::istream> configFile) {

        this->logger = logger;

        // First check if the stream is valid before moving it
        if (!configFile || !(*configFile)) {
            logger->error("Invalid or empty configuration stream provided");
            throw CreatureBuilderException("Invalid or empty configuration stream provided");
        }

        // Now that we've confirmed it's valid, we can safely move it
        this->configFile = std::move(configFile);

        if(!this->configFile->good()) {
            logger->error("config file isn't good");
            throw CreatureBuilderException("good() returned false while reading config file");
        }

        // Define the required config file fields
        requiredTopLevelFields = {
                "type", "name", "version", "starting_dmx_channel", "motors",
                "head_offset_max", "frame_time_ms", "position_min", "position_max",
                "description",
        };

        requiredServoFields = {
                "type", "id", "name", "output_pin", "min_pulse_us", "max_pulse_us",
                "smoothing_value", "inverted", "default_position"
        };

    }


    std::shared_ptr<Creature> CreatureBuilder::build() {

        logger->info("about to try to parse the config file");

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
        Creature::creature_type type = Creature::stringToCreatureType(j["type"]);
        if(type == Creature::invalid_creature) {
            logger->error("invalid creature type: {}", j["type"]);
            throw CreatureBuilderException(fmt::format("invalid creature type: {}", j["type"]));
        }

        std::shared_ptr<Creature> creature;

        switch(type) {
            case Creature::parrot:
                creature = std::make_shared<Parrot>();
                break;
            default:
                logger->error("unimplemented creature type: {}", j["type"]);
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
        creature->setFrameTimeMs(j["frame_time_ms"]);
        creature->setType(type);

        // Log that we've gotten this far
        logger->info("creature name is {} (version {}), at channel offset {}",
             creature->getName(), creature->getVersion(), creature->getStartingDmxChannel());

        for(auto& motor : j["motors"]) {

            // Validate the fields in this object
            for (const auto& fieldName : requiredServoFields) {
                checkJsonField(motor, fieldName);
            }
            logger->debug("looking at motor {}", motor["id"]);

            // Make sure we have a valid type for this motor
            Creature::motor_type motorType = Creature::stringToMotorType(motor["type"]);
            switch(motorType) {

                // Do this in it's own context since there's a var being created
                case Creature::servo: {
                    std::shared_ptr<Servo> servo = createServo(motor);
                    logger->debug("adding servo {}", servo->getId());
                    creature->addServo(servo->getId(), servo);
                    break;
                }
                default:
                    logger->error("invalid motor type: {}", motor["type"]);
                    throw CreatureBuilderException(fmt::format("invalid motor type: {}", motor["type"]));

            }

        }
        logger->debug("done processing motors");

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
    bool CreatureBuilder::isFileAccessible(std::shared_ptr<Logger> logger, const std::string& filename) {

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
    void CreatureBuilder::checkJsonField(const nlohmann::json& jsonObj, const std::string& fieldName) {
        if (!jsonObj.contains(fieldName)) {
            throw CreatureBuilderException("Missing required field: " + fieldName);
        }
    }

    std::unique_ptr<std::istream> CreatureBuilder::fileToStream(std::shared_ptr<Logger> logger, std::string filename) {

        logger->debug("turning {} into an istream", filename);

        // Make sure the file name is valid
        if(filename.empty()) {
            logger->error("no file name provided");
          throw CreatureBuilderException("no file name provided");
        }

        // Make sure we can both access and read the file
        if(!isFileAccessible(logger, filename)) {
            logger->error("file {} is not accessible", filename);
          throw CreatureBuilderException(fmt::format("file {} is not accessible", filename));
        }

        // Open the file and read its contents into a stringstream
        std::ifstream fileStream(filename);
        if (!fileStream) {
            logger->error("Failed to open file {}", filename);
            throw CreatureBuilderException(fmt::format("Failed to open file {}", filename));
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

} // creatures