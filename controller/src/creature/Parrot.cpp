
#include <cmath>

#include "controller-config.h"

#include "util/Result.h"
#include "util/ranges.h"
#include "util/thread_name.h"

#include "Creature.h"
#include "Parrot.h"

Parrot::Parrot(const std::shared_ptr<creatures::Logger> &logger) : Creature(logger) {

    // Set up our expectations for configuration for clean error checking
    requiredInputs = {"head_height", "head_tilt", "neck_rotate", "body_lean", "beak", "chest", "stand_rotate"};
    requiredServos = {"neck_left", "neck_right", "neck_rotate", "body_lean", "beak"};

    // Calculate the head offset max
    this->headOffsetMax = lround((double)(MAX_POSITION - MIN_POSITION) * (double)HEAD_OFFSET_MAX);
    logger->debug("the head offset max is {}", this->headOffsetMax);

    logger->info("Bawk!");
}

// Required for all Creature subclasses
creatures::Result<std::string> Parrot::performPreFlightCheck() {

    // List out the servos we found
    logger->debug("servos found:");
    for (const auto &[id, servo] : servos) {
        logger->debug("servo: {}", id);
    }

    // Do some error checking to make sure we have our servos
    for (const auto &requiredServo : requiredServos) {
        if (servos.find(requiredServo) == servos.end()) {
            auto errorMessage = fmt::format("missing required servo: {}", requiredServo);
            logger->critical(errorMessage);
            return creatures::Result<std::string>{
                creatures::ControllerError(creatures::ControllerError::InvalidConfiguration, errorMessage)};
        }
    }

    logger->debug("pre-flight check passed");
    return creatures::Result<std::string>{"Parrot is ready to fly!"};
}

uint16_t Parrot::convertToHeadHeight(uint16_t y) const {

    return convertRange(logger, y, MIN_POSITION, MAX_POSITION, MIN_POSITION + (this->headOffsetMax / 2),
                        MAX_POSITION - (this->headOffsetMax / 2));
}

int32_t Parrot::configToHeadTilt(uint16_t x) const {

    return convertRange(logger, x, MIN_POSITION, MAX_POSITION, 1 - (this->headOffsetMax / 2), this->headOffsetMax / 2);
}

head_position_t Parrot::calculateHeadPosition(uint16_t height, int32_t offset) {

    uint16_t right = height + offset;
    uint16_t left = height - offset;

    head_position_t headPosition;
    headPosition.left = left;
    headPosition.right = right;

    logger->trace("calculated head position: height: {}, offset: {} -> {}, {}", height, offset, right, left);

    return headPosition;
}

void Parrot::worker() {

    setThreadName("Parrot::worker");

    logger->info("Parrot initialized and ready for operation");

    while (!stop_requested.load()) {

        auto incoming = inputQueue->pop();

        logger->trace("creature got {} inputs", incoming.size());

        // Make sure we got the inputs we're expecting
        for (const auto &requiredInput : requiredInputs) {
            if (incoming.find(requiredInput) == incoming.end()) {
                logger->warn("missing required input: {}", requiredInput);
                continue;
            }
        }

#if DEBUG_CREATURE_WORKER_LOOP
        for (auto &input : incoming) {
            logger->debug("got input: {}", input.second.toString());
        }

        // Debug: Dump all of the servos
        logger->debug("server dump follows");
        for (const auto &[id, servo] : servos) {
            logger->trace("servo: {} -> {}", id, servo->getPosition());
        }
#endif

        u8 height = incoming["head_height"].getIncomingRequest();
        u8 tilt = incoming["head_tilt"].getIncomingRequest();
#if DEBUG_CREATURE_WORKER_LOOP
        logger->debug("head height: {}, head tilt: {}", height, tilt);
#endif

        u16 headHeight = convertToHeadHeight(Parrot::convertInputValueToServoValue(height));
        int32_t headTilt = configToHeadTilt(Parrot::convertInputValueToServoValue(tilt));

#if DEBUG_CREATURE_WORKER_LOOP
        logger->debug("head height: {}, head tilt: {}", headHeight, headTilt);
#endif

        head_position_t headPosition = calculateHeadPosition(headHeight, headTilt);

        // Update our servos so that they'll get picked up on the next frame
        servos["neck_left"]->move(headPosition.left);
        servos["neck_right"]->move(headPosition.right);
        servos["neck_rotate"]->move(
            Parrot::convertInputValueToServoValue(incoming["neck_rotate"].getIncomingRequest()));
        servos["body_lean"]->move(Parrot::convertInputValueToServoValue(incoming["body_lean"].getIncomingRequest()));
        servos["beak"]->move(Parrot::convertInputValueToServoValue(incoming["beak"].getIncomingRequest()));

#if DEBUG_CREATURE_WORKER_LOOP
        logger->debug("servos updated");
#endif
    }

    logger->info("Parrot worker thread stopped");
}
