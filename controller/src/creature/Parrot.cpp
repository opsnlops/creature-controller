
#include <cassert>
#include <climits>
#include <chrono>
#include <cmath>

#include "controller-config.h"

#include "util/ranges.h"
#include "util/thread_name.h"

#include "Parrot.h"
#include "Creature.h"

Parrot::Parrot(const std::shared_ptr<creatures::Logger>& logger)
        : Creature(logger) {

    // Calculate the head offset max
    this->headOffsetMax = lround((double) (MAX_POSITION - MIN_POSITION) * (double) HEAD_OFFSET_MAX);
    logger->debug("the head offset max is {}", this->headOffsetMax);

    logger->info("Bawk!");
}



uint16_t Parrot::convertToHeadHeight(uint16_t y) const {

    return convertRange(logger,
                        y,
                        MIN_POSITION,
                        MAX_POSITION,
                        MIN_POSITION + (this->headOffsetMax / 2),
                        MAX_POSITION - (this->headOffsetMax / 2));

}

int32_t Parrot::configToHeadTilt(uint16_t x) const {

    return convertRange(logger,
                        x,
                        MIN_POSITION,
                        MAX_POSITION,
                        1 - (this->headOffsetMax / 2),
                        this->headOffsetMax / 2);
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

     logger->info("Parrot reporting for duty!  📣🦜");

     while(!stop_requested.load()) {

         auto incoming = inputQueue->pop();

         logger->trace("creature got {} inputs", incoming.size());

         //for(auto& input : incoming) {
         //    logger->debug("got input: {}", input.second.toString());
        // }

         u8 height = incoming["head_height"].getIncomingRequest();
         u8 tilt = incoming["head_tilt"].getIncomingRequest();

         u16 headHeight = convertToHeadHeight(Parrot::convertInputValueToServoValue(height));
         int32_t headTilt = configToHeadTilt(Parrot::convertInputValueToServoValue(tilt));
         logger->trace("head height: {}, head tilt: {}", headHeight, headTilt);

         head_position_t headPosition = calculateHeadPosition(headHeight, headTilt);

         // Update our servos so they'll get picked up on the next frame
         servos["neck_left"]->move(headPosition.left);
         servos["neck_right"]->move(headPosition.right);
         servos["neck_rotate"]->move(Parrot::convertInputValueToServoValue(incoming["neck_rotate"].getIncomingRequest()));
         servos["body_lean"]->move(Parrot::convertInputValueToServoValue(incoming["body_lean"].getIncomingRequest()));
         servos["beak"]->move(Parrot::convertInputValueToServoValue(incoming["beak"].getIncomingRequest()));

     }

    logger->info("Parrot worker thread stopped");

 }
