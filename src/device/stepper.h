
#pragma once

#include "controller-config.h"

#include <cstdio>
#include "pico/stdlib.h"

#if USE_STEPPERS

class StepperState {

public:

    StepperState();

    /**
     * Used to determine how aggressive when we should switch to microsteps.
     *
     * Zero means disable.
     */
    u16 decelerationAggressiveness;

    u64 updatedFrame;

    // The controller requests things in whole steps
    u32 requestedSteps;

    u32 currentMicrostep;
    u32 desiredMicrostep;

    bool currentDirection;

    bool moveRequested;

    bool isHigh;
    bool isAwake;

    bool ms1State;
    bool ms2State;

    bool lowEndstop = false;
    bool highEndstop = false;

    /**
     * How many frames have we moved?
     *
     * This is just for metrics, it's not used for anything.
     */
    u64 actualSteps;


    /**
     * Which frame did we fall asleep at?
     */
    u64 startedSleepingAt;


    /**
     * How many frames of idle time do we have to wait before going to sleep?
     */
    u64 sleepAfterIdleFrames;

    /**
     * At which frame can we resume motion after wakeup?
     */
    u64 awakeAt;

    /**
     * How many frames do we have to wait to wake up?
     */
    u32 framesRequiredToWakeUp;


};

class Stepper {

public:
    Stepper(u8 slot, const char* name, u32 maxSteps, u16 decelerationAggressiveness,
            u32 sleepWakeupPauseTimeUs, u32 sleepAfterUs, bool inverted);
    int init();
    int start();

    /**
     * Which slot it is on the mux
     */
    u8 slot;

    const char* name;
    bool inverted;

    StepperState* state;

    u32 maxSteps;
    u32 maxMicrosteps;

    u16 decelerationAggressiveness;
    u32 sleepWakeupPauseTimeUs;
    u32 sleepAfterUs;

    /**
     * The number of frames needed to wake up from sleep
     */
    u32 sleepWakeupFrames;

    /**
     * After how many frames of no moment should we fall asleep?
     */
    u32 sleepAfterIdleFrames;

    [[nodiscard]] u8 getSlot() const;

    bool isInverted();

    [[nodiscard]] const char* getName() const;

    u16 getDecelerationAggressiveness();
    u32 getSleepWakeupPauseTimeUs();
    u32 getSleepAfterUs();
    u32 getSleepWakeupFrames();
    u32 getSleepAfterIdleFrames();

};

#endif