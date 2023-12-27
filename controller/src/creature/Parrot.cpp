
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

[[noreturn]] void Parrot::worker() {

    setThreadName("Parrot::worker");

     logger->info("Parrot reporting for duty!  📣🦜");


     for(EVER) {

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

 }

/**
 * This task is the main worker task for the parrot itself!
 *
 * It's an event loop. It pauses and waits for a signal from the controller
 * that a new frame has been received, and then does whatever it needs to do
 * to make magic.
 *
 * @param pvParameters
 */
 // TODO: This is an imporant thing that should be a Thread
 /*
portTASK_FUNCTION(creature_worker_task, pvParameters) {

auto info = (ParrotInfo *) pvParameters;
Controller* controller = info->controller;
Parrot *parrot = info->parrot;

// And give this small amount of memory back! :)
vPortFree(info);

uint32_t ulNotifiedValue;
uint8_t *currentFrame;
uint8_t numberOfJoints = parrot->getNumberOfJoints();
CreatureConfig* runningConfig;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

for (EVER) {

// Wait for the controller to signal to us that a new frame has been
// received off the wire
xTaskNotifyWait(0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);

// Fetch the current frame and configuration
currentFrame = controller->getCurrentFrame();
runningConfig = parrot->getRunningConfig();

#if DEBUG_CREATURE_POSITIONING

debug("Raw: {}", currentFrame[5]);

#endif



uint8_t height = currentFrame[INPUT_HEAD_HEIGHT];
uint8_t tilt = currentFrame[INPUT_HEAD_TILT];

uint16_t headHeight = parrot->convertToHeadHeight(Parrot::convertInputValueToServoValue(height));
int32_t headTilt = parrot->configToHeadTilt(Parrot::convertInputValueToServoValue(tilt));

verbose("head height: {}, head tilt: {}", headHeight, headTilt);

head_position_t headPosition = parrot->calculateHeadPosition(headHeight, headTilt);

parrot->joints[JOINT_NECK_LEFT] = headPosition.left;
parrot->joints[JOINT_NECK_RIGHT] = headPosition.right;
parrot->joints[JOINT_NECK_ROTATE] = Parrot::convertInputValueToServoValue(currentFrame[INPUT_NECK_ROTATE]);
parrot->joints[JOINT_BODY_LEAN] = Parrot::convertInputValueToServoValue(currentFrame[INPUT_BODY_LEAN]);
parrot->joints[JOINT_BEAK] = Parrot::convertInputValueToServoValue(currentFrame[INPUT_BEAK]);
parrot->joints[JOINT_CHEST] = Parrot::convertInputValueToServoValue(currentFrame[INPUT_CHEST]);

/ *
parrot->joints[JOINT_NECK_ROTATE] = Parrot::convertRange(currentFrame[INPUT_NECK_ROTATE],
                                                         0,
                                                         UCHAR_MAX,
                                                       0,
                                                       runningConfig->getStepperConfig(STEPPER_NECK_ROTATE)->maxSteps);

parrot->joints[JOINT_BODY_LEAN] = Parrot::convertRange(currentFrame[INPUT_BODY_LEAN],
                                                         0,
                                                         UCHAR_MAX,
                                                         0,
                                                         runningConfig->getStepperConfig(STEPPER_BODY_LEAN)->maxSteps);

parrot->joints[JOINT_STAND_ROTATE] = Parrot::convertRange(currentFrame[INPUT_STAND_ROTATE],
                                                         0,
                                                         UCHAR_MAX,
                                                         0,
                                                         runningConfig->getStepperConfig(STEPPER_STAND_ROTATE)->maxSteps);
* /

// Request these positions from the controller
controller->requestServoPosition(SERVO_NECK_LEFT, parrot->joints[JOINT_NECK_LEFT]);
controller->requestServoPosition(SERVO_NECK_RIGHT, parrot->joints[JOINT_NECK_RIGHT]);
controller->requestServoPosition(SERVO_NECK_ROTATE, parrot->joints[JOINT_NECK_ROTATE]);
controller->requestServoPosition(SERVO_BODY_LEAN, parrot->joints[JOINT_BODY_LEAN]);
controller->requestServoPosition(SERVO_BEAK, parrot->joints[JOINT_BEAK]);
controller->requestServoPosition(SERVO_CHEST, parrot->joints[JOINT_CHEST]);

//controller->requestStepperPosition(STEPPER_NECK_ROTATE, parrot->joints[JOINT_NECK_ROTATE]);
//controller->requestStepperPosition(STEPPER_BODY_LEAN, parrot->joints[JOINT_BODY_LEAN]);
//controller->requestStepperPosition(STEPPER_STAND_ROTATE, parrot->joints[JOINT_STAND_ROTATE]);
}
#pragma clang diagnostic pop
}
*/