
#pragma once

#include "controller-config.h"
#include "namespace-stuffs.h"

#include <climits>
#include <memory>
#include <unistd.h>
#include <thread>


#include "creature/config.h"
#include "controller/controller.h"

class Creature {

public:

    // Valid creature types
    enum types {
        parrot,
        wled_light,
        skunk
    };

    explicit Creature();

    /**
     * Storage space for the joints!
     *
     * Initialize it to the size of the joints.
     */
    u16* joints;

    /**
     * Set up the controller
     */
    virtual void init(Controller* controller) = 0;

    /**
     * Start running!
     */
    virtual void start() = 0;

    /**
     * Returns a task to be notified when there is a new frame to process
     *
     * @return a `TaskHandle_t` pointing to the task
     */
     // TODO This is a thread now
    std::thread getWorkerTaskHandle();

    /**
     * Converts a value that input handlers speaks (0-255) to one the servo controller
     * uses (MIN_POSITION to MAX_POSITION).
     *
     * @param inputValue a `uint8_t` to convert to the servo mappings
     * @return a value between MIN_POSITION and MAX_POSITION
     */
    static u16 convertInputValueToServoValue(u8 inputValue);

    /**
     * Gets the number of joints that this creature has
     *
     * @return the number of joints
     */
    [[nodiscard]] u8 getNumberOfJoints() const;

    [[nodiscard]] u8 getNumberOfServos() const;

#if USE_STEPPERS
    [[nodiscard]] u8 getNumberOfSteppers() const;
#endif

protected:

    Controller* controller;
    // TODO: Thread
    std::thread workerTaskHandle;

    uint8_t numberOfServos;
    uint8_t numberOfJoints;

#if USE_STEPPERS
    uint8_t numberOfSteppers;
#endif

};
