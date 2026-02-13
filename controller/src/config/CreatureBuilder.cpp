
/**
 * @file CreatureBuilder.cpp
 * @brief Implementation of the CreatureBuilder class
 */

// Standard library includes
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

// Third-party includes
#include <nlohmann/json.hpp>
#include <utility>

// Project includes
#include "config/CreatureBuilder.h"
#include "config/CreatureBuilderException.h"
#include "config/UARTDevice.h"
#include "controller/Input.h"
#include "creature/Creature.h"
#include "creature/Parrot.h"
#include "device/Servo.h"
#include "device/ServoSpecifier.h"
#include "logging/Logger.h"
#include "util/Result.h"

// Use json as an alias for nlohmann::json for cleaner code
using json = nlohmann::json;

namespace creatures::config {

/**
 * @brief Constructor for the CreatureBuilder class
 * @param logger Shared pointer to a logger instance
 * @param configFile Path to the creature configuration file
 */
CreatureBuilder::CreatureBuilder(std::shared_ptr<Logger> logger, std::string configFile)
    : BaseBuilder(std::move(logger), std::move(configFile)) {
    // Define the required config file fields
    requiredTopLevelFields = {
        "id",
        "type",
        "name",
        "version",
        "channel_offset",
        "motors",
        "head_offset_max",
        "servo_frequency",
        "position_min",
        "position_max",
        "description",
        "audio_channel",
        "mouth_slot",
    };

    requiredServoFields = {"type",          "id",
                           "name",          "output_module",
                           "output_header", "min_pulse_us",
                           "max_pulse_us",  "smoothing_value",
                           "inverted",      "default_position"};

    requiredInputFields = {"name", "slot", "width"};
}

/**
 * @brief Build a creature from the configuration file
 * @return Result containing either a creature pointer or an error
 */
Result<std::shared_ptr<creatures::creature::Creature>> CreatureBuilder::build() {
    logger->info("Parsing creature configuration file");

    // Verify file accessibility
    auto fileResult = isFileAccessible(logger, fileName);
    if (!fileResult.isSuccess()) {
        return Result<std::shared_ptr<creatures::creature::Creature>>{ControllerError(
            ControllerError::InternalError, "Unable to determine if the creature config file is accessible")};
    }

    // Check if file is readable
    if (!fileResult.getValue().value()) {
        return Result<std::shared_ptr<creatures::creature::Creature>>{
            ControllerError(ControllerError::InvalidConfiguration, fmt::format("File {} is not accessible", fileName))};
    }

    // Load the configuration file
    auto configFileResult = loadFile(logger, fileName);
    if (!configFileResult.isSuccess()) {
        auto errorMessage = fmt::format("Unable to open {} for reading", fileName);
        logger->warn(errorMessage);
        return Result<std::shared_ptr<creatures::creature::Creature>>{
            ControllerError(ControllerError::InvalidData, errorMessage)};
    }
    auto configFile = std::move(configFileResult.getValue().value());

    // Parse JSON content
    json j;
    try {
        j = json::parse(configFile);
        logger->debug("Configuration file successfully parsed");
    } catch (json::parse_error &e) {
        auto errorMessage = fmt::format("Unable to parse creature config file: {}", e.what());
        logger->error(errorMessage);
        return Result<std::shared_ptr<creatures::creature::Creature>>{
            ControllerError(ControllerError::InvalidData, errorMessage)};
    }

    // Verify JSON is an object
    if (!j.is_object()) {
        auto errorMessage = "JSON is not an object in creature config file";
        logger->error(errorMessage);
        return Result<std::shared_ptr<creatures::creature::Creature>>{
            ControllerError(ControllerError::InvalidData, errorMessage)};
    }

    // Validate required top-level fields
    for (const auto &fieldName : requiredTopLevelFields) {
        auto fieldResult = checkJsonField(j, fieldName);
        if (!fieldResult.isSuccess()) {
            auto errorMessage = fieldResult.getError().value().getMessage();
            logger->error(errorMessage);
            return Result<std::shared_ptr<creatures::creature::Creature>>{
                ControllerError(ControllerError::InvalidData, errorMessage)};
        }
    }

    // Validate and get creature type
    creatures::creature::Creature::creature_type type = creatures::creature::Creature::stringToCreatureType(j["type"]);
    std::string string_type = j["type"];

    if (type == creatures::creature::Creature::invalid_creature) {
        auto errorMessage = fmt::format("Invalid creature type: {}", string_type);
        logger->critical(errorMessage);
        return Result<std::shared_ptr<creatures::creature::Creature>>{
            ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
    }

    // Create a creature instance based on type
    std::shared_ptr<creatures::creature::Creature> creature;
    switch (type) {
    case creatures::creature::Creature::parrot:
        creature = std::make_shared<Parrot>(logger);
        break;
    default:
        auto errorMessage = fmt::format("Unimplemented creature type: {}", string_type);
        logger->error(errorMessage);
        return Result<std::shared_ptr<creatures::creature::Creature>>{
            ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
    }

    // The servo frequency is shared across all servos for the creature
    u16 servo_frequency = j["servo_frequency"];

    // Configure creature properties
    creature->setName(j["name"]);
    creature->setId(j["id"]);
    creature->setVersion(j["version"]);
    creature->setDescription(j["description"]);
    creature->setChannelOffset(j["channel_offset"]);
    creature->setAudioChannel(j["audio_channel"]);
    creature->setMouthSlot(j["mouth_slot"]);
    creature->setPositionMin(j["position_min"]);
    creature->setPositionMax(j["position_max"]);
    creature->setHeadOffsetMax(j["head_offset_max"]);
    creature->setServoUpdateFrequencyHz(servo_frequency);
    creature->setType(type);

    logger->info("Configuring creature: {} (version {}), at channel offset {}", creature->getName(),
                 creature->getVersion(), creature->getChannelOffset());

    // Process motors configuration
    for (auto &motor : j["motors"]) {
        std::string id_string = motor["id"];
        std::string type_string = motor["type"];

        // Validate motor fields
        for (const auto &fieldName : requiredServoFields) {
            if (auto fieldResult = checkJsonField(motor, fieldName); !fieldResult.isSuccess()) {
                auto errorMessage = fieldResult.getError().value().getMessage();
                logger->error(errorMessage);
                return Result<std::shared_ptr<creatures::creature::Creature>>{
                    ControllerError(ControllerError::InvalidData, errorMessage)};
            }
        }

        logger->debug("Processing motor: {}", id_string);

        // Validate motor type
        creatures::creature::Creature::motor_type motorType =
            creatures::creature::Creature::stringToMotorType(motor["type"]);

        switch (motorType) {
        case creatures::creature::Creature::servo: {
            auto servoResult = createServo(motor, servo_frequency);
            if (!servoResult.isSuccess()) {
                auto errorMessage = servoResult.getError().value().getMessage();
                logger->error(errorMessage);
                return Result<std::shared_ptr<creatures::creature::Creature>>{
                    ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
            }

            auto servo = servoResult.getValue().value();
            logger->debug("Adding servo - ID: {}, Name: {}", servo->getId(), servo->getName());
            creature->addServo(servo->getId(), servo);
            break;
        }
        case creatures::creature::Creature::dynamixel: {
            // Dynamixel motors use different JSON fields
            std::vector<std::string> requiredDynamixelFields = {"type",         "id",
                                                                "name",         "output_module",
                                                                "dxl_id",       "min_position",
                                                                "max_position", "smoothing_value",
                                                                "inverted",     "default_position"};
            for (const auto &fieldName : requiredDynamixelFields) {
                if (auto fieldResult = checkJsonField(motor, fieldName); !fieldResult.isSuccess()) {
                    auto errorMessage = fieldResult.getError().value().getMessage();
                    logger->error(errorMessage);
                    return Result<std::shared_ptr<creatures::creature::Creature>>{
                        ControllerError(ControllerError::InvalidData, errorMessage)};
                }
            }

            std::string dxl_id = motor["id"];
            std::string dxl_name = motor["name"];
            std::string output_module_str = motor["output_module"];
            u16 dxl_bus_id = motor["dxl_id"];
            u16 min_position = motor["min_position"];
            u16 max_position = motor["max_position"];
            float smoothing_value = motor["smoothing_value"];
            bool dxl_inverted = motor["inverted"];
            std::string dxl_default_str = motor["default_position"];

            UARTDevice::module_name output_location = UARTDevice::stringToModuleName(output_module_str);
            if (output_location == UARTDevice::invalid_module) {
                auto errorMessage = fmt::format("Invalid Dynamixel module: {}", output_module_str);
                logger->error(errorMessage);
                return Result<std::shared_ptr<creatures::creature::Creature>>{
                    ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
            }

            // Use ServoSpecifier with dynamixel type — pin field stores the bus ID
            ServoSpecifier output = {output_location, dxl_bus_id, creatures::creature::Creature::dynamixel};

            // Calculate default position
            u16 default_position = 0;
            auto requestedDefault = creatures::creature::Creature::stringToDefaultPositionType(dxl_default_str);
            switch (requestedDefault) {
            case creatures::creature::Creature::center:
                default_position = min_position + ((max_position - min_position) / 2);
                break;
            case creatures::creature::Creature::min:
                default_position = min_position;
                break;
            case creatures::creature::Creature::max:
                default_position = max_position;
                break;
            case creatures::creature::Creature::invalid_position: {
                auto errorMessage = fmt::format("Invalid default position for Dynamixel: {}", dxl_default_str);
                logger->error(errorMessage);
                return Result<std::shared_ptr<creatures::creature::Creature>>{
                    ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
            }
            }

            // Create Servo with Dynamixel type — min/max pulse fields store position range
            auto servo = std::make_shared<Servo>(logger, dxl_id, dxl_name, output, min_position, max_position,
                                                 smoothing_value, dxl_inverted, servo_frequency, default_position);

            logger->debug("Adding Dynamixel servo - ID: {}, Name: {}, Bus ID: {}", servo->getId(), servo->getName(),
                          dxl_bus_id);
            creature->addServo(servo->getId(), servo);
            break;
        }
        default:
            auto errorMessage = fmt::format("Invalid motor type: {}", type_string);
            logger->error(errorMessage);
            return Result<std::shared_ptr<creatures::creature::Creature>>{
                ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
        }
    }

    logger->debug("Finished processing motors");

    // Process inputs configuration
    if (j.contains("inputs")) {
        for (auto &input : j["inputs"]) {
            // Validate input fields
            for (const auto &fieldName : requiredInputFields) {
                auto fieldCheckResult = checkJsonField(input, fieldName);
                if (!fieldCheckResult.isSuccess()) {
                    auto errorMessage = fieldCheckResult.getError().value().getMessage();
                    logger->error(errorMessage);
                    return Result<std::shared_ptr<creatures::creature::Creature>>{
                        ControllerError(ControllerError::InvalidData, errorMessage)};
                }
            }

            std::string inputName = input["name"];
            u16 inputSlot = input["slot"];
            u8 inputWidth = input["width"];

            // Validate DMX slot range (1-512)
            if (inputSlot > 512) {
                auto errorMessage = fmt::format("Input slot {} is out of range (max 512)", inputSlot);
                logger->error(errorMessage);
                return Result<std::shared_ptr<creatures::creature::Creature>>{
                    ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
            }

            creature->addInput(creatures::Input(inputName, inputSlot, inputWidth, 0UL));
            logger->debug("Added input: {} at slot {}", inputName, inputSlot);
        }
    }

    logger->info("Creature configuration complete");
    return Result{creature};
}

/**
 * @brief Creates a servo from its JSON configuration
 * @param j JSON object containing servo configuration
 * @param servo_frequency The frequency at which to drive the servo
 * @return Result containing either a servo pointer or an error
 */
Result<std::shared_ptr<Servo>> CreatureBuilder::createServo(const json &j, u16 servo_frequency) {
    // Validate motor type
    creatures::creature::Creature::motor_type type = creatures::creature::Creature::stringToMotorType(j["type"]);

    if (type == creatures::creature::Creature::invalid_motor) {
        auto errorMessage = fmt::format("Invalid motor type: {}", j["type"].get<std::string>());
        logger->error(errorMessage);
        return Result<std::shared_ptr<Servo>>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
    }

    // Extract servo configuration values
    std::string id = j["id"];
    std::string name = j["name"];
    std::string output_module_as_string = j["output_module"];
    u16 output_header = j["output_header"];
    u16 min_pulse_us = j["min_pulse_us"];
    u16 max_pulse_us = j["max_pulse_us"];
    float smoothing_value = j["smoothing_value"];
    bool inverted = j["inverted"];
    std::string parseDefault = j["default_position"];

    // Calculate default position based on specified type
    u16 default_position = 0;
    creatures::creature::Creature::default_position_type requestedDefault =
        creatures::creature::Creature::stringToDefaultPositionType(parseDefault);

    // Convert module string to enum
    UARTDevice::module_name output_location = UARTDevice::stringToModuleName(output_module_as_string);
    if (output_location == creatures::config::UARTDevice::invalid_module) {
        auto errorMessage = fmt::format("Invalid servo module: {}", output_module_as_string);
        logger->error(errorMessage);
        return Result<std::shared_ptr<Servo>>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
    }

    ServoSpecifier output = {output_location, output_header};

    // Set default position based on requested type
    switch (requestedDefault) {
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
        auto errorMessage = fmt::format("Invalid default position: {}", parseDefault);
        logger->error(errorMessage);
        return Result<std::shared_ptr<Servo>>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
    }

    // Create and return the servo
    auto servo = std::make_shared<Servo>(logger, id, name, output, min_pulse_us, max_pulse_us, smoothing_value,
                                         inverted, servo_frequency, default_position);

    return Result<std::shared_ptr<Servo>>{servo};
}

} // namespace creatures::config
