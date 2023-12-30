
#include <cmath>
#include <utility>


#include "controller-config.h"

#include "creature/Creature.h"
#include "creature/CreatureException.h"
#include "logging/Logger.h"

namespace creatures::creature {

    Creature::Creature(std::shared_ptr<creatures::Logger> logger) : logger(logger) {

        this->controller = nullptr;
        //this->workerTaskHandle = nullptr;
        this->numberOfJoints = 0;

        // Wipe out the maps and init them
        servos.clear();
        steppers.clear();

        logger->debug("Creature() called!");
    }

    void Creature::init(const std::shared_ptr<Controller>& c) {
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

    u16 Creature::convertInputValueToServoValue(u8 inputValue) {

        // TODO: Play with the results if we do bit shifts instead (8 -> 10)

        u16 servoRange = MAX_POSITION - MIN_POSITION;

        double movementPercentage = (double) inputValue / (double) UCHAR_MAX;
        auto servoValue = (u16) (round((double) servoRange * movementPercentage) + MIN_POSITION);

        logger->trace("mapped {} -> {}", inputValue, servoValue);

        return servoValue;
    }

    std::vector<creatures::ServoPosition> Creature::getRequestedServoPositions() {

        std::vector<creatures::ServoPosition> positions;
        positions.reserve(servos.size());

        // Quickly walk the servos and return the number of ticks we want next
        for (const auto &[key, servo]: servos) {
            positions.emplace_back(servo->getOutputLocation(), servo->getCurrentMicroseconds());
        }

        return positions;

    }

    std::vector<creatures::ServoConfig> Creature::getServoConfigs() {

        std::vector<creatures::ServoConfig> servoConfigs;
        servoConfigs.reserve(servos.size());

        // Walk all of our servos and make a ServoConfig for each
        for (const auto &[key, servo]: servos) {
            ServoConfig servoConfig = ServoConfig(logger, servo);
            servoConfigs.emplace_back(servoConfig);
        }

        return servoConfigs;
    }


    void Creature::calculateNextServoPositions() {

        // Quickly walk the servos and return the number of ticks we want next
        for (const auto &[key, servo]: servos) {
            servo->calculateNextTick();
        }
    }


    std::shared_ptr<Servo> Creature::getServo(const std::string &id) {
        return servos[id];
    }

    u8 Creature::getNumberOfJoints() const {
        return numberOfJoints;
    }

    u8 Creature::getNumberOfServos() const {
        return servos.size();
    }

#if USE_STEPPERS

    u8 Creature::getNumberOfSteppers() const {
        return steppers.size();
    }

#endif

    Creature::creature_type Creature::stringToCreatureType(const std::string &typeStr) {
        if (typeStr == "parrot") return parrot;
        if (typeStr == "wled_light") return wled_light;
        if (typeStr == "skunk") return skunk;
        return invalid_creature;
    }

    Creature::motor_type Creature::stringToMotorType(const std::string &typeStr) {
        if (typeStr == "servo") return servo;
        if (typeStr == "stepper") return stepper;
        return invalid_motor;
    }

    Creature::default_position_type Creature::stringToDefaultPositionType(const std::string &typeStr) {
        if (typeStr == "min") return min;
        if (typeStr == "max") return max;
        if (typeStr == "center") return center;
        return invalid_position;
    }

    void Creature::addServo(std::string id, const std::shared_ptr<Servo> &servo) {

        // Whoa there, this shouldn't happen
        if (servos.contains(id)) {
            std::string errorMessage = fmt::format("Servo with id {} already exists!", id);
            logger->critical(errorMessage);
            throw creatures::CreatureException(errorMessage);
        }

        logger->info("adding servo {} ({})", servo->getName(), id);
        servos[id] = servo;
    }


    const std::string &Creature::getName() const {
        return name;
    }

    void Creature::setName(const std::string &name) {
        Creature::name = name;
    }

    const std::string &Creature::getVersion() const {
        return version;
    }

    void Creature::setVersion(const std::string &_version) {
        Creature::version = _version;
    }

    const std::string &Creature::getDescription() const {
        return description;
    }

    void Creature::setDescription(const std::string &description) {
        Creature::description = description;
    }

    Creature::creature_type Creature::getType() const {
        return type;
    }

    void Creature::setType(Creature::creature_type type) {
        Creature::type = type;
    }

    u16 Creature::getServoUpdateFrequencyHz() const {
        return servoUpdateFrequencyHz;
    }

    void Creature::setServoUpdateFrequencyHz(u16 hertz) {
        Creature::servoUpdateFrequencyHz = hertz;
    }

    u8 Creature::getStartingDmxChannel() const {
        return startingDmxChannel;
    }

    void Creature::setStartingDmxChannel(u8 _startingDmxChannel) {
        Creature::startingDmxChannel = _startingDmxChannel;
    }

    u16 Creature::getDmxUniverse() const {
        return dmxUniverse;
    }

    void Creature::setDmxUniverse(u16 _dmxUniverse) {
        Creature::dmxUniverse = _dmxUniverse;
    }

    u16 Creature::getPositionMin() const {
        return positionMin;
    }

    void Creature::setPositionMin(u16 positionMin) {
        Creature::positionMin = positionMin;
    }

    u16 Creature::getPositionMax() const {
        return positionMax;
    }

    void Creature::setPositionMax(u16 positionMax) {
        Creature::positionMax = positionMax;
    }

    u16 Creature::getPositionDefault() const {
        return positionDefault;
    }

    void Creature::setPositionDefault(u16 positionDefault) {
        Creature::positionDefault = positionDefault;
    }

    float Creature::getHeadOffsetMax() const {
        return headOffsetMax;
    }

    void Creature::setHeadOffsetMax(float headOffsetMax) {
        Creature::headOffsetMax = headOffsetMax;
    }

    void Creature::addInput(const creatures::Input &input) {
        inputs.push_back(input);
    }

    std::vector<creatures::Input> Creature::getInputs() const {
        return inputs;
    }

} // creatures::creature