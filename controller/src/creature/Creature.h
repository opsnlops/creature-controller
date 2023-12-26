
#pragma once

#include <climits>
#include <memory>
#include <unistd.h>
#include <thread>
#include <unordered_map>

#include "controller-config.h"

#include "controller/Controller.h"
#include "controller/commands/tokens/ServoPosition.h"
#include "creature/Creature.h"
#include "device/Servo.h"
#include "device/Stepper.h"
#include "logging/Logger.h"

class Creature {

public:

    // Allow our builder to touch us in ways only friends can
    friend class CreatureBuilder;

    // Valid creature types
    enum creature_type {
        parrot,
        wled_light,
        skunk,
        invalid_creature // (Not really a valid type) 😅
    };

    enum motor_type {
        servo,
        stepper,
        invalid_motor
    };

    enum default_position_type {
        min,
        max,
        center,
        invalid_position
    };

    explicit Creature(std::shared_ptr<creatures::Logger> logger);

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
     * @param inputValue a `u8` to convert to the servo mappings
     * @return a value between MIN_POSITION and MAX_POSITION
     */
     u16 convertInputValueToServoValue(u8 inputValue);

    /**
     * Gets the number of joints that this creature has
     *
     * @return the number of joints
     */
    [[nodiscard]] u8 getNumberOfJoints() const;

    void addServo(std::string id, const std::shared_ptr<Servo>& servo);
    [[nodiscard]] u8 getNumberOfServos() const;

    void addStepper(std::string id, std::shared_ptr<Stepper> stepper);
    [[nodiscard]] u8 getNumberOfSteppers() const;


    static creature_type stringToCreatureType(const std::string& typeStr);
    static motor_type stringToMotorType(const std::string& typeStr);
    static default_position_type stringToDefaultPositionType(const std::string& typeStr);


    /**
     * @brief Get the current requested positions of the servos
     *
     * This walks the map of servos and returns a vector of the number of each ticks
     * that the creature would like the servos set to. This is called from the controller's
     * worker thread.
     *
     * @return a `std::vector<creatures::ServoPosition>` of the requested positions
     */
    std::vector<creatures::ServoPosition> getRequestedServoPositions();

    /**
     * @brief Ask all of the servos to calculate their next positions
     */
    void calculateNextServoPositions();


    // Getters for all of the things
    const std::string &getName() const;
    const std::string &getVersion() const;
    const std::string &getDescription() const;
    creature_type getType() const;
    u8 getStartingDmxChannel() const;
    u16 getDmxUniverse() const;
    u16 getPositionMin() const;
    u16 getPositionMax() const;
    u16 getPositionDefault() const;
    float getHeadOffsetMax() const;
    u16 getServoUpdateFrequencyHz() const;

    std::shared_ptr<Servo> getServo(const std::string& id);
    std::shared_ptr<Stepper> getStepper(std::string id);

    // Setters for the things
    void setName(const std::string &name);
    void setVersion(const std::string &version);
    void setDescription(const std::string &description);;
    void setType(creature_type type);
    void setStartingDmxChannel(u8 startingDmxChannel);
    void setDmxUniverse(u16 dmxUniverse);
    void setPositionMin(u16 positionMin);
    void setPositionMax(u16 positionMax);
    void setPositionDefault(u16 positionDefault);
    void setHeadOffsetMax(float headOffsetMax);
    void setServoUpdateFrequencyHz(u16 hertz);


protected:
    std::string name;
    std::string version;
    std::string description;
    creature_type type;

    u16 positionMin;
    u16 positionMax;
    u16 positionDefault;
    float headOffsetMax;
    u16 servoUpdateFrequencyHz;

    u16 dmxUniverse;
    u8 startingDmxChannel;


    std::shared_ptr<Controller> controller;
    std::unordered_map<std::string, std::shared_ptr<Servo>> servos;
    std::unordered_map<std::string, std::shared_ptr<Stepper>> steppers;

    std::thread workerTaskHandle;

    u8 numberOfJoints;

    std::shared_ptr<creatures::Logger> logger;

};
