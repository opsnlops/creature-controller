
#include <cmath>

#include "controller-config.h"

#include "util/Result.h"

#include "Creature.h"
#include "Crow.h"

Crow::Crow(const std::shared_ptr<creatures::Logger> &logger) : Creature(logger) {

    // Start with Parrot's inputs/servos as a baseline -- adjust when real hardware is wired
    requiredInputs = {"head_height", "head_tilt", "neck_rotate", "body_lean", "beak", "chest", "stand_rotate"};
    requiredServos = {"neck_left", "neck_right", "neck_rotate", "body_lean", "beak"};

    logger->info("Caw!");
}

void Crow::applyConfig(const nlohmann::json &config) {
    float headOffsetMaxPercent = config.at("head_offset_max");
    head.emplace(logger, headOffsetMaxPercent, positionMin, positionMax);
    logger->debug("Crow: configured DifferentialHead with head_offset_max = {}", headOffsetMaxPercent);
}

creatures::Result<std::string> Crow::performPreFlightCheck() {

    logger->debug("servos found:");
    for (const auto &[id, servo] : servos) {
        logger->debug("servo: {}", id);
    }

    for (const auto &requiredServo : requiredServos) {
        if (servos.find(requiredServo) == servos.end()) {
            auto errorMessage = fmt::format("missing required servo: {}", requiredServo);
            logger->critical(errorMessage);
            return creatures::Result<std::string>{
                creatures::ControllerError(creatures::ControllerError::InvalidConfiguration, errorMessage)};
        }
    }

    if (!head.has_value()) {
        auto errorMessage = "DifferentialHead not configured (missing head_offset_max in config?)";
        logger->critical(errorMessage);
        return creatures::Result<std::string>{
            creatures::ControllerError(creatures::ControllerError::InvalidConfiguration, errorMessage)};
    }

    logger->debug("pre-flight check passed");
    return creatures::Result<std::string>{"Crow is ready to fly!"};
}

void Crow::mapInputsToServos(const std::unordered_map<std::string, creatures::Input> &inputs) {

    u8 height = inputs.at("head_height").getIncomingRequest();
    u8 tilt = inputs.at("head_tilt").getIncomingRequest();

    u16 headHeight = head->convertToHeadHeight(convertInputValueToServoValue(height));
    int32_t headTilt = head->convertToHeadTilt(convertInputValueToServoValue(tilt));

    auto headPosition = head->calculateHeadPosition(headHeight, headTilt);

    servos["neck_left"]->move(headPosition.left);
    servos["neck_right"]->move(headPosition.right);
    servos["neck_rotate"]->move(convertInputValueToServoValue(inputs.at("neck_rotate").getIncomingRequest()));
    servos["body_lean"]->move(convertInputValueToServoValue(inputs.at("body_lean").getIncomingRequest()));
    servos["beak"]->move(convertInputValueToServoValue(inputs.at("beak").getIncomingRequest()));
}
