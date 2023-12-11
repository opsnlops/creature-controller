
#pragma once

#include <climits>
#include <memory>
#include <unistd.h>
#include <thread>
#include <unordered_map>

#include "controller-config.h"
#include "namespace-stuffs.h"

#include "controller/controller.h"
#include "creature/creature.h"
#include "device/servo.h"
#include "device/stepper.h"

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
     * @param inputValue a `u8` to convert to the servo mappings
     * @return a value between MIN_POSITION and MAX_POSITION
     */
    static u16 convertInputValueToServoValue(u8 inputValue);

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


    // Getters for all of the things
    const std::string &getName() const;
    const std::string &getVersion() const;
    const std::string &getDescription() const;
    creature_type getType() const;
    u8 getStartingDmxChannel() const;
    u16 getPositionMin() const;
    u16 getPositionMax() const;
    u16 getPositionDefault() const;
    float getHeadOffsetMax() const;
    u16 getServoFrequency() const;

    std::shared_ptr<Servo> getServo(const std::string& id);
    std::shared_ptr<Stepper> getStepper(std::string id);

    // Setters for the things
    void setName(const std::string &name);
    void setVersion(const std::string &version);
    void setDescription(const std::string &description);;
    void setType(creature_type type);
    void setStartingDmxChannel(u8 startingDmxChannel);
    void setPositionMin(u16 positionMin);
    void setPositionMax(u16 positionMax);
    void setPositionDefault(u16 positionDefault);
    void setHeadOffsetMax(float headOffsetMax);
    void setServoFrequency(u16 servoFrequency);


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
    std::unordered_map<std::string, std::shared_ptr<Servo>> servos;
    std::unordered_map<std::string, std::shared_ptr<Stepper>> steppers;

    // TODO: Thread
    std::thread workerTaskHandle;

    u8 numberOfJoints;


};
