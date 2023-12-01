
#pragma once

#include "controller-config.h"
#include "namespace-stuffs.h"

#include <climits>
#include <memory>
#include <unistd.h>
#include <thread>

#include "creature/creature.h"

#include "controller/controller.h"

class Creature {

public:

    // Valid creature types
    enum creature_type {
        parrot,
        wled_light,
        skunk,
        invalid_type
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
    virtual void init(std::shared_ptr<Controller> controller) = 0;

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

    creature_type stringToType(const std::string& typeStr);

protected:

    std::string name;
    std::string version;
    std::string description;
    creature_type type;
    u8 startingDmxChannel;
    u16 positionMin;
    u16 positionMax;
    u16 positionDefault;
    float headOffsetMax;
    u16 servoFrequency;

    std::shared_ptr<Controller> controller;

    // TODO: Thread
    std::thread workerTaskHandle;

    u8 numberOfServos;
    u8 numberOfJoints;
    u8 numberOfSteppers;

};
