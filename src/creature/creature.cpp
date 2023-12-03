
#include <cmath>

#include "namespace-stuffs.h"
#include "controller-config.h"


#include "creature/creature.h"



Creature::Creature() {

    this->controller = nullptr;
    //this->workerTaskHandle = nullptr;
    this->numberOfJoints = 0;

    // Wipe out the maps and init them
    servos.clear();
    steppers.clear();

    debug("Creature() called!");
}

// TODO: Thread time
//TaskHandle_t Creature::getWorkerTaskHandle() {
//    return workerTaskHandle;
//}

void Creature::init(std::shared_ptr<Controller> c) {
    this->controller = c;

    debug("init done, controller exists");
}

uint16_t Creature::convertInputValueToServoValue(uint8_t inputValue) {

    // TODO: Play with the results if we do bit shifts instead (8 -> 10)

    uint16_t servoRange = MAX_POSITION - MIN_POSITION;

    double movementPercentage = (double)inputValue / (double)UCHAR_MAX;
    auto servoValue = (uint16_t)(round((double)servoRange * movementPercentage) + MIN_POSITION);

    trace("mapped {} -> {}", inputValue, servoValue);

    return servoValue;
}


uint8_t Creature::getNumberOfJoints() const {
    return numberOfJoints;
}

uint8_t Creature::getNumberOfServos() const {
    return servos.size();
}

#if USE_STEPPERS
uint8_t Creature::getNumberOfSteppers() const {
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

    info("adding servo {} ({})", servo->getName(), id);
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

u16 Creature::getServoFrequency() const {
    return servoFrequency;
}

void Creature::setServoFrequency(u16 servoFrequency) {
    Creature::servoFrequency = servoFrequency;
}

