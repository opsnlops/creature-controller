
#include <cmath>

#include "controller-config.h"
#include "namespace-stuffs.h"


#if USE_STEPPERS

#include "stepper.h"

StepperState::StepperState() {

    updatedFrame = 0L;

    requestedSteps = 0;

    currentMicrostep = 0;
    desiredMicrostep = 0;
    currentDirection = false;

    isHigh = false;
    isAwake = true;

    moveRequested = false;

    // Default to full steps
    ms1State = false;
    ms2State = false;

    lowEndstop = false;
    highEndstop = false;

    actualSteps = 0L;

    startedSleepingAt = 0L;
    sleepAfterIdleFrames = 0L;

}


Stepper::Stepper(uint8_t slot, const char* name, uint32_t maxSteps, uint16_t decelerationAggressiveness,
                 uint32_t sleepWakeupPauseTimeUs, uint32_t sleepAfterUs, bool inverted) {

    trace("setting up a new stepper");

    this->state = new StepperState();
    this->slot = slot;
    this->name = name;
    this->maxSteps = maxSteps;
    this->maxMicrosteps = maxSteps * STEPPER_MICROSTEP_MAX;
    this->decelerationAggressiveness = decelerationAggressiveness;
    this->sleepWakeupPauseTimeUs = sleepWakeupPauseTimeUs;
    this->sleepAfterUs = sleepAfterUs;
    this->inverted = inverted;

    this->state->decelerationAggressiveness = decelerationAggressiveness;

    // Figure out how many frames we need to wake up and sleep after
    this->sleepWakeupFrames = ceil(this->sleepWakeupPauseTimeUs / (double)STEPPER_LOOP_PERIOD_IN_US);
    this->sleepAfterIdleFrames = ceil(this->sleepAfterUs / (double)STEPPER_LOOP_PERIOD_IN_US);

    // Start out awake
    this->state->awakeAt = 0;
    this->state->isAwake = true;
    this->state->framesRequiredToWakeUp = this->sleepWakeupFrames;
    this->state->sleepAfterIdleFrames = this->sleepAfterIdleFrames;

    info("set up stepper on slot {}: name: {}, max_steps: {}, deceleration: {}, wake frames: {}, idle after: {}, inverted: {}",
         slot, name, maxSteps, decelerationAggressiveness, this->sleepWakeupFrames, this->sleepAfterIdleFrames, inverted ? "yes" : "no");

}

int Stepper::init() {
    return 1;

}

int Stepper::start() {
    return 1;
}


const char* Stepper::getName() const {
    return this->name;
}

uint8_t Stepper::getSlot() const {
    return this->slot;
}

bool Stepper::isInverted() const {
    return this->inverted;
}

uint16_t Stepper::getDecelerationAggressiveness() const {
    return this->decelerationAggressiveness;
}

uint32_t Stepper::getSleepWakeupPauseTimeUs() const {
    return this->sleepWakeupPauseTimeUs;
}

uint32_t Stepper::getSleepAfterUs() const {
    return this->sleepAfterUs;
}

uint32_t Stepper::getSleepWakeupFrames() {
    return this->sleepWakeupFrames;
}

uint32_t Stepper::getSleepAfterIdleFrames() {
    return this->sleepAfterIdleFrames;
}

#endif