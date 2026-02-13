
#include <cmath>

#include "controller-config.h"

#include "config/UARTDevice.h"
#include "creature/Creature.h"
#include "creature/CreatureException.h"
#include "logging/Logger.h"

namespace creatures::creature {

Creature::Creature(const std::shared_ptr<creatures::Logger> &logger) : logger(logger) {

    this->controller = nullptr;
    this->numberOfJoints = 0;

    // Wipe out the maps and init them
    servos.clear();
    steppers.clear();

    logger->debug("Creature() called!");
}

void Creature::init(const std::shared_ptr<Controller> &c) {
    this->controller = c;

    // Save a reference to the input queue for quick actions
    this->inputQueue = c->getInputQueue();

    logger->debug("init done, creature exists");
}

void Creature::start() {

    logger->info("starting up the creature working thread");
    workerThread = std::thread(&Creature::worker, this);
    workerThread.detach();
}

void Creature::shutdown() {
    logger->info("asking the creature worker thread to stop");
    stop_requested.store(true);
}

u16 Creature::convertInputValueToServoValue(u8 inputValue) {

    // TODO: Play with the results if we do bit shifts instead (8 -> 10)

    u16 servoRange = MAX_POSITION - MIN_POSITION;

    double movementPercentage = (double)inputValue / (double)UCHAR_MAX;
    auto servoValue = (u16)(round((double)servoRange * movementPercentage) + MIN_POSITION);

    logger->trace("mapped {} -> {}", inputValue, servoValue);

    return servoValue;
}

std::vector<creatures::ServoPosition>
Creature::getRequestedServoPositions(creatures::config::UARTDevice::module_name module) {

    // Create a vector to hold the filtered positions into
    std::vector<creatures::ServoPosition> positions;

    // Quickly walk the servos and return the number of ticks we want next
    for (const auto &[key, servo] : servos) {
        if (servo->getOutputModule() == module) {
            positions.emplace_back(servo->getOutputLocation(), servo->getCurrentMicroseconds());
        }
    }

    return positions;
}

std::vector<creatures::ServoConfig> Creature::getServoConfigs(creatures::config::UARTDevice::module_name module) {

    std::vector<creatures::ServoConfig> servoConfigs;
    servoConfigs.reserve(servos.size());

    // Walk all of our servos and make a ServoConfig for each
    for (const auto &[key, servo] : servos) {

        // If it's not on the module we're looking for, skip it
        if (servo->getOutputModule() != module) {
            continue;
        }

        ServoConfig servoConfig = ServoConfig(logger, servo);
        servoConfigs.emplace_back(servoConfig);
    }

    return servoConfigs;
}

void Creature::calculateNextServoPositions() {

    // Quickly walk the servos and return the number of ticks we want next
    for (const auto &[key, servo] : servos) {
        servo->calculateNextTick();
    }
}

std::shared_ptr<Servo> Creature::getServo(const std::string &servoName) { return servos[servoName]; }

u8 Creature::getNumberOfJoints() const { return numberOfJoints; }

u8 Creature::getNumberOfServos() const { return servos.size(); }

#if USE_STEPPERS

u8 Creature::getNumberOfSteppers() const { return steppers.size(); }

#endif

Creature::creature_type Creature::stringToCreatureType(const std::string &typeStr) {
    if (typeStr == "parrot")
        return parrot;
    if (typeStr == "wled_light")
        return wled_light;
    if (typeStr == "skunk")
        return skunk;
    return invalid_creature;
}

Creature::motor_type Creature::stringToMotorType(const std::string &typeStr) {
    if (typeStr == "servo")
        return servo;
    if (typeStr == "dynamixel")
        return dynamixel;
    if (typeStr == "stepper")
        return stepper;
    return invalid_motor;
}

Creature::default_position_type Creature::stringToDefaultPositionType(const std::string &typeStr) {
    if (typeStr == "min")
        return min;
    if (typeStr == "max")
        return max;
    if (typeStr == "center")
        return center;
    return invalid_position;
}

// Get a servo by name
void Creature::addServo(std::string servoName, const std::shared_ptr<Servo> &servo) {

    // Whoa there, this shouldn't happen
    if (servos.contains(servoName)) {
        std::string errorMessage = fmt::format("Servo with name {} already exists!", servoName);
        logger->critical(errorMessage);
        throw creatures::CreatureException(errorMessage);
    }

    logger->info("adding servo on {} (mod {}, pin {})", servo->getName(),
                 creatures::config::UARTDevice::moduleNameToString(servo->getOutputLocation().module),
                 servo->getOutputLocation().pin);
    servos[servoName] = servo;
}

const std::string &Creature::getName() const { return name; }

void Creature::setName(const std::string &_name) { Creature::name = _name; }

const std::string &Creature::getId() const { return id; }

void Creature::setId(const std::string &_id) { Creature::id = _id; }

const std::string &Creature::getVersion() const { return version; }

void Creature::setVersion(const std::string &_version) { Creature::version = _version; }

const std::string &Creature::getDescription() const { return description; }

void Creature::setDescription(const std::string &_description) { Creature::description = _description; }

Creature::creature_type Creature::getType() const { return type; }

void Creature::setType(Creature::creature_type _type) { Creature::type = _type; }

u16 Creature::getServoUpdateFrequencyHz() const { return servoUpdateFrequencyHz; }

void Creature::setServoUpdateFrequencyHz(u16 hertz) { Creature::servoUpdateFrequencyHz = hertz; }

u16 Creature::getChannelOffset() const { return channelOffset; }

void Creature::setChannelOffset(u16 _channelOffset) { Creature::channelOffset = _channelOffset; }

u8 Creature::getAudioChannel() const { return audioChannel; }

void Creature::setAudioChannel(u8 _audioChannel) { Creature::audioChannel = _audioChannel; }

u8 Creature::getMouthSlot() const { return mouthSlot; }

void Creature::setMouthSlot(u8 _mouthSlot) { Creature::mouthSlot = _mouthSlot; }

u16 Creature::getPositionMin() const { return positionMin; }

void Creature::setPositionMin(u16 _positionMin) { Creature::positionMin = _positionMin; }

u16 Creature::getPositionMax() const { return positionMax; }

void Creature::setPositionMax(u16 _positionMax) { Creature::positionMax = _positionMax; }

u16 Creature::getPositionDefault() const { return positionDefault; }

void Creature::setPositionDefault(u16 _positionDefault) { Creature::positionDefault = _positionDefault; }

float Creature::getHeadOffsetMax() const { return headOffsetMax; }

void Creature::setHeadOffsetMax(float _headOffsetMax) { Creature::headOffsetMax = _headOffsetMax; }

void Creature::addInput(const creatures::Input &input) { inputs.push_back(input); }

std::vector<creatures::Input> Creature::getInputs() const { return inputs; }

} // namespace creatures::creature