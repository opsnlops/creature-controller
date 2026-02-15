
#pragma once

#include <atomic>
#include <climits>
#include <memory>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "controller-config.h"

#include "config/UARTDevice.h"
#include "controller/Controller.h"
#include "controller/Input.h"
#include "controller/commands/tokens/ServoConfig.h"
#include "controller/commands/tokens/ServoPosition.h"
#include "creature/Creature.h"
#include "creature/MotorType.h"
#include "device/Servo.h"
#include "device/ServoSpecifier.h"
#include "device/Stepper.h"
#include "logging/Logger.h"
#include "util/MessageQueue.h"

class Controller;

namespace creatures::creature {

class Creature {

  public:
    // Allow our builder to touch us in ways only friends can
    // friend class CreatureBuilder;

    // Valid creature types
    enum class creature_type {
        parrot,
        crow,
        invalid_creature
    };

    // Motor type is defined in creature/MotorType.h to avoid circular includes
    using motor_type = creatures::creature::motor_type;

    enum default_position_type { min, max, center, invalid_position };

    explicit Creature(const std::shared_ptr<creatures::Logger> &logger);
    virtual ~Creature() = default;

    /**
     * Set up the controller
     */
    void init(const std::shared_ptr<Controller> &controller);

    /**
     * Start running!
     */
    void start();

    /**
     * Request that the creature stop running
     */
    void shutdown();

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

    void addServo(std::string servoName, const std::shared_ptr<Servo> &servo);

    [[nodiscard]] u8 getNumberOfServos() const;

    void addStepper(std::string id, std::shared_ptr<Stepper> stepper);

    [[nodiscard]] u8 getNumberOfSteppers() const;

    static creature_type stringToCreatureType(const std::string &typeStr);

    static motor_type stringToMotorType(const std::string &typeStr);

    static default_position_type stringToDefaultPositionType(const std::string &typeStr);

    /**
     * @brief Get the current requested positions of the servos
     *
     * This walks the map of servos and returns a vector of the number of each ticks
     * that the creature would like the servos set to. This is called from the controller's
     * worker thread.
     *
     * @param module the module to get the positions for
     *
     * @return a `std::vector<creatures::ServoPosition>` of the requested positions
     */
    std::vector<creatures::ServoPosition> getRequestedServoPositions(creatures::config::UARTDevice::module_name module);

    /**
     * @brief Gets a ServoConfig for each servo on a module
     *
     * This is used to generate a configuration that's sent over to the firmware in response
     * to an INIT message. It allows the creature to tell the firmware the limits of each of
     * the servos so it can also do error checking on it's side.
     *
     * @return `std::vector<creatures::ServoConfig>` of the servos that are connected to a specific module
     */
    std::vector<creatures::ServoConfig> getServoConfigs(creatures::config::UARTDevice::module_name module);

    /**
     * @brief Ask all of the servos to calculate their next positions
     */
    void calculateNextServoPositions();

    // Getters for all of the things
    const std::string &getName() const;

    const std::string &getId() const;

    const std::string &getVersion() const;

    const std::string &getDescription() const;

    creature_type getType() const;

    u16 getChannelOffset() const;

    u8 getAudioChannel() const;

    u8 getMouthSlot() const;

    u16 getPositionMin() const;

    u16 getPositionMax() const;

    u16 getPositionDefault() const;

    u16 getServoUpdateFrequencyHz() const;

    std::vector<creatures::Input> getInputs() const;

    std::shared_ptr<Servo> getServo(const std::string &servoName);

    std::shared_ptr<Stepper> getStepper(std::string id);

    // Setters for the things
    void setName(const std::string &name);

    void setId(const std::string &id);

    void setVersion(const std::string &version);

    void setDescription(const std::string &description);

    void setType(creature_type type);

    void setChannelOffset(u16 channelOffset);

    void setAudioChannel(u8 audioChannel);

    void setMouthSlot(u8 mouthSlot);

    void setPositionMin(u16 positionMin);

    void setPositionMax(u16 positionMax);

    void setPositionDefault(u16 positionDefault);

    void setServoUpdateFrequencyHz(u16 hertz);

    void addInput(const creatures::Input &input);

    // Perform a pre-flight check to make sure everything is set up correctly
    virtual Result<std::string> performPreFlightCheck() = 0;

    /**
     * Called after all common properties are set by CreatureBuilder.
     * Subclasses can override to extract creature-specific JSON parameters.
     */
    virtual void applyConfig(const nlohmann::json &config) { (void)config; }

  protected:
    std::atomic<bool> stop_requested = false;
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    creature_type type;

    u16 positionMin;
    u16 positionMax;
    u16 positionDefault;
    u16 servoUpdateFrequencyHz;

    u16 channelOffset;
    u8 audioChannel;
    u8 mouthSlot;

    /**
     * Inputs as defined in the config file for this creature
     */
    std::vector<creatures::Input> inputs;
    std::vector<std::string> requiredInputs = {}; // Make sure this is set in the child class

    /**
     * Message queue to send inputs to the creature
     */
    std::shared_ptr<creatures::MessageQueue<std::unordered_map<std::string, creatures::Input>>> inputQueue;

    std::vector<creatures::ServoPosition> servoPositions;
    std::shared_ptr<Controller> controller;

    /**
     * A map of the servos that this creature has, by id. This is used to look up the servos
     * by name when we're setting their positions.
     *
     * It's by name to make it easier on me when programming creatures. This isn't a mistake
     * that it's not a ServoSpecifier.
     */
    std::unordered_map<std::string, std::shared_ptr<Servo>> servos;
    std::vector<std::string> requiredServos = {}; // Must populate this in the child class

    std::unordered_map<std::string, std::shared_ptr<Stepper>> steppers;

    std::thread workerThread;
    void worker();

    /**
     * Map incoming inputs to servo positions. Called by the worker loop
     * each time a new set of inputs arrives from the input queue.
     */
    virtual void mapInputsToServos(const std::unordered_map<std::string, creatures::Input> &inputs) = 0;

    u8 numberOfJoints;

    std::shared_ptr<creatures::Logger> logger;
};

} // namespace creatures::creature