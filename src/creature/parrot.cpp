
#include <climits>
#include <cmath>

#include "namespace-stuffs.h"
#include "controller-config.h"

#include "creature/config.h"
#include "util/ranges.h"

#include "parrot.h"
#include "creature.h"

Parrot::Parrot()
        : Creature() {

    // Calculate the head offset max
    this->headOffsetMax = lround((double) (MAX_POSITION - MIN_POSITION) * (double) HEAD_OFFSET_MAX);
    debug("the head offset max is {}", this->headOffsetMax);

    this->numberOfServos = 6;
    //this->numberOfSteppers = 3;

    info("Bawk!");
}

CreatureConfig* Parrot::getDefaultConfig() {

    auto defaultConfig = new CreatureConfig(CREATURE_NAME, 50, 6, 1, 1);

    defaultConfig->setServoConfig(SERVO_NECK_LEFT,
                                  new ServoConfig("Neck Left", 1250, 2250,
                                                  0.90, DEFAULT_POSITION, false));
    defaultConfig->setServoConfig(SERVO_NECK_RIGHT,
                                  new ServoConfig("Neck Right", 800, 1800,
                                                  0.90, DEFAULT_POSITION, true));
    defaultConfig->setServoConfig(SERVO_NECK_ROTATE,
                                  new ServoConfig("Neck Rotate", 1400, 1900,
                                                  0.95, DEFAULT_POSITION, false));
    defaultConfig->setServoConfig(SERVO_BODY_LEAN,
                                  new ServoConfig("Body Lean", 1475, 1950,
                                                  0.96, MIN_POSITION,true));
    defaultConfig->setServoConfig(SERVO_BEAK,
                                  new ServoConfig("Beak", 1600, 2350,
                                                  0.4, MIN_POSITION, true));
    defaultConfig->setServoConfig(SERVO_CHEST,
                                  new ServoConfig("Chest", 250, 2500,
                                                  0.99, DEFAULT_POSITION, false));

    // "Max Steps" is in full steps (use the datasheet from the stepper to know how big that is)

    /*
     * StepperConfig(uint8_t slot, const char* name, uint32_t maxSteps, uint16_t decelerationAggressiveness,
                  uint32_t sleepWakeupPauseTimeUs, uint32_t sleepAfterUs, bool inverted)
     */

    /*
     * The datasheet for the stepper driver we're using says that it needs 1ms to wake back up again
     */
#define WAIT_AFTER_WAKEUP_TIME 2000

    /*
    defaultConfig->setStepperConfig(STEPPER_NECK_ROTATE,
                                    new StepperConfig(STEPPER_NECK_ROTATE,
                                                      "Neck Rotate",
                                                      20p,
                                                      8,
                                                      WAIT_AFTER_WAKEUP_TIME,
                                                      5000 * 1000,        // 5s
                                                      false));

    defaultConfig->setStepperConfig(STEPPER_BODY_LEAN,
                                    new StepperConfig(STEPPER_BODY_LEAN,
                                                      "Body Lean",
                                                      400,
                                                      2,
                                                      WAIT_AFTER_WAKEUP_TIME,
                                                      1000 * 1000,
                                                      false));

    defaultConfig->setStepperConfig(STEPPER_STAND_ROTATE,
                                    new StepperConfig(STEPPER_STAND_ROTATE,
                                                      "Stand Rotate",
                                                      300,
                                                      8,
                                                      WAIT_AFTER_WAKEUP_TIME,
                                                      2000 * 1000,
                                                      false));
    */
    // Make our running defaultConfig point to this
    this->runningConfig = defaultConfig;

    return defaultConfig;

}

void Parrot::init(Controller* controller) {
    debug("starting creature init");

    this->controller = controller;
    this->numberOfJoints = 7;

    // Initialize the array on for the joints on the heap
    this->joints = (uint16_t *) malloc(sizeof(uint16_t) * numberOfJoints);
}

void Parrot::start() {

    // Make sure we have a controller
    assert(this->controller != nullptr);

    // Declare this on the heap, so it lasts once start() goes out of scope
    auto info = (ParrotInfo *) malloc(sizeof(ParrotInfo));
    info->controller = this->controller;
    info->parrot = this;

    // Fire off our worker task
    // TODO: Thread
    /*
    xTaskCreate(creature_worker_task,
                "creature_worker_task",
                2048,
                (void *) info,
                1,
                &workerTaskHandle);
    */

    debug("parrot started!");
}

uint16_t Parrot::convertToHeadHeight(uint16_t y) const {

    return convertRange(y,
                        MIN_POSITION,
                        MAX_POSITION,
                        MIN_POSITION + (this->headOffsetMax / 2),
                        MAX_POSITION - (this->headOffsetMax / 2));

}

int32_t Parrot::configToHeadTilt(uint16_t x) const {

    return convertRange(x,
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

    trace("calculated head position: height: {}, offset: {} -> {}, {}", height, offset, right, left);

    return headPosition;

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