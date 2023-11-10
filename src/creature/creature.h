
#pragma once

#include "controller-config.h"
#include "namespace-stuffs.h"

#include <climits>
#include <memory>
#include <unistd.h>


#include "creature/config.h"
#include "controller/controller.h"

class Creature {

public:

    explicit Creature();

    /**
     * Storage space for the joints!
     *
     * Initialize it to the size of the joints.
     */
    uint16_t* joints;

    /**
     * Set up the controller
     */
    virtual void init(Controller* controller) = 0;

    /**
     * Start running!
     */
    virtual void start() = 0;

    /**
     * Create the default config for this creature
     *
     * @return a non-customized configuration for this creature
     */
    virtual CreatureConfig* getDefaultConfig() = 0;

    /**
     * Get the "running" config in the Cisco IOS sense
     *
     * @return a pointer to our running config
     */
    CreatureConfig* getRunningConfig();

    /**
     * Returns a task to be notified when there is a new frame to process
     *
     * @return a `TaskHandle_t` pointing to the task
     */
     // TODO This is a thread now
    //TaskHandle_t getWorkerTaskHandle();

    /**
     * Converts a value that input handlers speaks (0-255) to one the servo controller
     * uses (MIN_POSITION to MAX_POSITION).
     *
     * @param inputValue a `uint8_t` to convert to the servo mappings
     * @return a value between MIN_POSITION and MAX_POSITION
     */
    static uint16_t convertInputValueToServoValue(uint8_t inputValue);

    /**
     * Gets the number of joints that this creature has
     *
     * @return the number of joints
     */
    [[nodiscard]] uint8_t getNumberOfJoints() const;

    [[nodiscard]] uint8_t getNumberOfServos() const;

#if USE_STEPPERS
    [[nodiscard]] uint8_t getNumberOfSteppers() const;
#endif

protected:

    Controller* controller;
    // TODO: Thread
    //TaskHandle_t workerTaskHandle;

    uint8_t numberOfServos;
    uint8_t numberOfJoints;

    // This is the config that we're currently using. "Running" in the Cisco sense.
    CreatureConfig* runningConfig;

#if USE_STEPPERS
    uint8_t numberOfSteppers;
#endif

};
