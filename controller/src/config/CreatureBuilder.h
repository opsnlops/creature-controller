
/**
 * @file CreatureBuilder.h
 * @brief Defines the CreatureBuilder class for building creature instances from JSON configuration files
 */
#pragma once

// Standard library includes
#include <string>
#include <vector>
#include <memory>

// Third-party includes
#include <nlohmann/json.hpp>

// Project includes
#include "config/BaseBuilder.h"
#include "creature/Creature.h"
#include "device/Servo.h"
#include "logging/Logger.h"
#include "util/Result.h"

namespace creatures::config {

    /**
     * @class CreatureBuilder
     * @brief Reads a JSON file from the filesystem and creates a Creature configuration
     *
     * This class loads and parses a JSON configuration file to build a complete
     * creature with configured motors, inputs, and other properties. This replaces
     * the earlier approach of using hardcoded values in the Pi's EEPROM.
     */
    class CreatureBuilder : public BaseBuilder {
    public:
        /**
         * @brief Constructor for the CreatureBuilder
         * @param logger Shared pointer to a logger instance
         * @param configFile Path to the creature configuration file
         */
        CreatureBuilder(std::shared_ptr<Logger> logger, std::string configFile);

        /**
         * @brief Default destructor
         */
        ~CreatureBuilder() = default;

        /**
         * @brief Parses the creature configuration from the JSON file
         * @return Result containing either a creature pointer or an error
         */
        Result<std::shared_ptr<creatures::creature::Creature>> build();

    private:
        // Lists of required fields in the configuration
        std::vector<std::string> requiredTopLevelFields;
        std::vector<std::string> requiredServoFields;
        std::vector<std::string> requiredInputFields;

        /**
         * @brief Creates a servo from its JSON configuration
         * @param j JSON object containing servo configuration
         * @param servo_frequency The frequency at which to drive the servo
         * @return Result containing either a servo pointer or an error
         */
        Result<std::shared_ptr<Servo>> createServo(const nlohmann::json& j, u16 servo_frequency);
    };

} // namespace creatures::config
