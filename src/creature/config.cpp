
#include "creature/config.h"

#include "controller-config.h"
#include "namespace-stuffs.h"


CreatureConfig::CreatureConfig(const char* name, uint32_t servoFrequencyHz,
                               uint8_t numberOfServos, uint8_t numberOfSteppers, uint16_t dmxBaseChannel) {

    debug("creating a new creatureConfig");

    // Initialize the name
    memset(this->name, '\0', CREATURE_CONFIG_NAME_MAX_SIZE + 1);
    strncpy(this->name, name, CREATURE_CONFIG_NAME_MAX_SIZE);

    this->servoFrequencyHz = servoFrequencyHz;
    this->numberOfServos = numberOfServos;
    this->dmxBaseChannel = dmxBaseChannel;

    // Zero out the servo and stepper arrays
    memset(this->servoConfigs, '\0', sizeof(ServoConfig*) * MAX_NUMBER_OF_SERVOS);

#if USE_STEPPERS
    this->numberOfSteppers = numberOfSteppers;
    memset(this->stepperConfigs, '\0', sizeof(StepperConfig*) * MAX_NUMBER_OF_STEPPERS);
#endif
}

void CreatureConfig::setServoConfig(uint8_t index, ServoConfig* config) {
    this->servoConfigs[index] = config;
}

ServoConfig* CreatureConfig::getServoConfig(uint8_t servoNumber) {
    return this->servoConfigs[servoNumber];
}

uint32_t CreatureConfig::getServoFrequencyHz() {
    return this->servoFrequencyHz;
}

uint16_t CreatureConfig::getDmxBaseChannel() {
    return this->dmxBaseChannel;
}

uint8_t CreatureConfig::getNumberOfServos() {
    return this->numberOfServos;
}

char* CreatureConfig::getName() {
    return this->name;
}

#if USE_STEPPERS
void CreatureConfig::setStepperConfig(uint8_t index, StepperConfig* config) {
    this->stepperConfigs[index] = config;
}

StepperConfig* CreatureConfig::getStepperConfig(uint8_t stepperNumber) {
    return this->stepperConfigs[stepperNumber];
}

uint8_t CreatureConfig::getNumberOfSteppers() {
    return this->numberOfSteppers;
}
#endif


ServoConfig::ServoConfig() {

    // Initialize the name
    memset(this->name, '\0', CREATURE_CONFIG_NAME_MAX_SIZE + 1);
    strncpy(this->name, "???", CREATURE_CONFIG_NAME_MAX_SIZE);

    this->minPulseUs = 0;
    this->maxPulseUs = 0;
    this->defaultPosition = DEFAULT_POSITION;
    this->smoothingValue = 0.0f;
    this->inverted = false;
}


ServoConfig::ServoConfig(const char* name, uint16_t minPulseUs, uint16_t maxPulseUs,
                         float smoothingValue, uint16_t defaultPosition, bool inverted) {

    // Initialize the name
    memset(this->name, '\0', CREATURE_CONFIG_NAME_MAX_SIZE + 1);
    strncpy(this->name, name, CREATURE_CONFIG_NAME_MAX_SIZE);

    this->minPulseUs = minPulseUs;
    this->maxPulseUs = maxPulseUs;
    this->smoothingValue = smoothingValue;
    this->defaultPosition = defaultPosition;
    this->inverted = inverted;

}

#if USE_STEPPERS
StepperConfig::StepperConfig() {

    // Initialize the name
    memset(this->name, '\0', CREATURE_CONFIG_NAME_MAX_SIZE + 1);
    strncpy(this->name, "???", CREATURE_CONFIG_NAME_MAX_SIZE);

    this->maxSteps = 0;
    this->maxMicrosteps = 0;
    this->decelerationAggressiveness = 0;
    this->sleepWakeupPauseTimeUs = 0;
    this->sleepAfterUs = 0;
    this->inverted = false;
}


StepperConfig::StepperConfig(uint8_t slot, const char* name, uint32_t maxSteps, uint16_t decelerationAggressiveness,
                             uint32_t sleepWakeupPauseTimeUs, uint32_t sleepAfterUs, bool inverted) {

    // Initialize the name
    memset(this->name, '\0', CREATURE_CONFIG_NAME_MAX_SIZE + 1);
    strncpy(this->name, name, CREATURE_CONFIG_NAME_MAX_SIZE);

    this->slot = slot;
    this->maxSteps = maxSteps;
    this->maxMicrosteps = maxSteps * STEPPER_MICROSTEP_MAX;
    this->decelerationAggressiveness = decelerationAggressiveness;
    this->sleepWakeupPauseTimeUs = sleepWakeupPauseTimeUs;
    this->sleepAfterUs = sleepAfterUs;
    this->inverted = inverted;
}
#endif