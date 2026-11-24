
#include "controller-config.h"

#include "util/Result.h"

#include "Creature.h"
#include "Crow.h"

Crow::Crow(const std::shared_ptr<creatures::Logger> &logger) : Creature(logger) {

    // Each axis is both an input and a motor, and they share the same name
    requiredInputs = {"lean", "beak", "neck_twist", "head_twist", "neck_lean", "wings"};
    requiredServos = requiredInputs;

    logger->info("Caw!");
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

    // The crow is entirely Dynamixel
    for (const auto &[id, servo] : servos) {
        if (servo->getMotorType() != creatures::creature::motor_type::dynamixel) {
            auto errorMessage = fmt::format("servo {} is not a Dynamixel, and a crow only has Dynamixels", id);
            logger->critical(errorMessage);
            return creatures::Result<std::string>{
                creatures::ControllerError(creatures::ControllerError::InvalidConfiguration, errorMessage)};
        }
    }

    logger->debug("pre-flight check passed");
    return creatures::Result<std::string>{"Crow is ready to fly!"};
}

void Crow::mapInputsToServos(const std::unordered_map<std::string, creatures::Input> &inputs) {

    for (const auto &axis : requiredInputs) {

        // The worker has already warned about any inputs that are missing, so leave that motor where it is
        auto input = inputs.find(axis);
        if (input == inputs.end()) {
            continue;
        }

        servos.at(axis)->move(convertInputValueToServoValue(input->second.getIncomingRequest()));
    }
}
