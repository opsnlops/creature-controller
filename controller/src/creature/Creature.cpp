
#include <cmath>
#include <utility>


#include "controller-config.h"

#include "creature/Creature.h"
#include "creature/CreatureException.h"
#include "logging/Logger.h"

Creature::Creature(std::shared_ptr<creatures::Logger> logger) : logger(logger) {

    this->controller = nullptr;
    //this->workerTaskHandle = nullptr;
    this->numberOfJoints = 0;

    // Wipe out the maps and init them
    servos.clear();
    steppers.clear();

    logger->debug("Creature() called!");
}

// TODO: Thread time
//TaskHandle_t Creature::getWorkerTaskHandle() {
//    return workerTaskHandle;
//}

void Creature::init(std::shared_ptr<Controller> c) {
    this->controller = std::move(c);

    logger->debug("init done, controller exists");
}

u16 Creature::convertInputValueToServoValue(u8 inputValue) {

    // TODO: Play with the results if we do bit shifts instead (8 -> 10)

    u16 servoRange = MAX_POSITION - MIN_POSITION;

    double movementPercentage = (double)inputValue / (double)UCHAR_MAX;
    auto servoValue = (u16)(round((double)servoRange * movementPercentage) + MIN_POSITION);

    logger->trace("mapped {} -> {}", inputValue, servoValue);

    return servoValue;
}


std::shared_ptr<Servo> Creature::getServo(const std::string& id) {
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

Creature::creature_type Creature::stringToCreatureType(const std::string& typeStr) {
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

void Creature::addServo(std::string id, const std::shared_ptr<Servo>& servo) {

    // Whoa there, this shouldn't happen
    if(servos.contains(id)) {
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

void Creature::setVersion(const std::string &version) {
    Creature::version = version;
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

u8 Creature::getStartingDmxChannel() const {
    return startingDmxChannel;
}

void Creature::setStartingDmxChannel(u8 startingDmxChannel) {
    Creature::startingDmxChannel = startingDmxChannel;
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

u16 Creature::getFrameTimeMs() const {
    return frameTimeMs;
}

void Creature::setFrameTimeMs(u16 frameTimeMs) {
    Creature::frameTimeMs = frameTimeMs;
}

