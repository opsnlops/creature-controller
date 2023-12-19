

#include <climits>
#include <utility>

#include "controller-config.h"

#include "device/Servo.h"

#include "controller/StepperHandler.h"
#include "Controller.h"

#include "creature/Creature.h"

uint32_t number_of_moves = 0;

// TODO: Threads now
//extern TaskHandle_t controllerHousekeeperTaskHandle;
//extern TaskHandle_t controller_motor_setup_task_handle;
//BaseType_t isrPriorityTaskWoken = pdFALSE;

// Initialize the static members
Servo *Controller::servos[MAX_NUMBER_OF_SERVOS] = {};
uint8_t Controller::numberOfServosInUse = 0;
uint32_t Controller::numberOfPWMWraps = 0;

#if USE_STEPPERS

Stepper *Controller::steppers[MAX_NUMBER_OF_STEPPERS] = {};
uint8_t Controller::numberOfSteppersInUse = 0;
//struct repeating_timer stepper_timer;

#endif

Controller::Controller(std::shared_ptr<creatures::Logger> logger) : logger(std::move(logger)) {

    logger->debug("setting up the controller");

    //creatureWorkerTaskHandle = nullptr;
    online = true;
    receivedFirstFrame = false;

}

void Controller::init(std::shared_ptr<Creature> _creature) {

    this->creature = std::move(_creature);
    this->numberOfChannels = DMX_NUMBER_OF_CHANNELS;

    // Initialize all the slots in the controller
    for (auto &servo: servos) {
        servo = nullptr;
    }

#if USE_STEPPERS
    for (auto &stepper: steppers) {
        stepper = nullptr;
    }
#endif

    // Set up the servos
    //
    // TODO: This is easier now that we have actual STL objects
    //
    logger->debug("building servo objects");
    for (int i = 0; i < this->creature->getNumberOfServos(); i++) {
        //ServoConfig *servo = this->config->getServoConfig(i);
        //initServo(i, servo->name, servo->minPulseUs, servo->maxPulseUs,
        //          servo->smoothingValue, servo->defaultPosition, servo->inverted);
    }

#if USE_STEPPERS
    // Set up the steppers
    logger->debug("building stepper objects");
    for (int i = 0; i < this->creature->getNumberOfSteppers(); i++) {
        /*
        StepperConfig *stepper = this->creature->getStepperConfig(i);
        initStepper(i, stepper->name, stepper->maxSteps, stepper->decelerationAggressiveness,
                    stepper->sleepWakeupPauseTimeUs, stepper->sleepAfterUs, stepper->inverted);
        */
    }
#endif

    // Declare some space on the heap for our current frame buffer
    currentFrame = (uint8_t *) malloc(sizeof(uint8_t) * numberOfChannels);

    // Set the currentFrame buffer to the middle value as a safe-ish default
    for (int i = 0; i < numberOfChannels; i++) {
        currentFrame[i] = UCHAR_MAX / 2;
    }

    logger->debug("setting up the stepper pins");

#if USE_STEPPERS
    // Output Pins
    configureGPIO(STEPPER_STEP_PIN, true, false);
    configureGPIO(STEPPER_DIR_PIN, true, false);
    configureGPIO(STEPPER_MS1_PIN, true, false);
    configureGPIO(STEPPER_MS2_PIN, true, false);
    configureGPIO(STEPPER_A0_PIN, true, false);
    configureGPIO(STEPPER_A1_PIN, true, false);
    configureGPIO(STEPPER_A2_PIN, true, false);
    configureGPIO(STEPPER_LATCH_PIN, true, true);       // The latch is active low
    configureGPIO(STEPPER_SLEEP_PIN, true, false);

    // Input pins
    configureGPIO(STEPPER_FAULT_PIN, false, false);
    //configureGPIO(STEPPER_END_S_LOW_PIN, false, false);
    //configureGPIO(STEPPER_END_S_HIGH_PIN, false, false);

    // TODO: Make these use the Pi pins
    //gpio_set_function(STEPPER_END_S_LOW_PIN, GPIO_FUNC_SIO);
    //gpio_set_function(STEPPER_END_S_LOW_PIN, GPIO_FUNC_SIO);
    //gpio_set_dir(STEPPER_END_S_LOW_PIN, false);
    //gpio_set_dir(STEPPER_END_S_HIGH_PIN, false);

    logger->debug("done setting up the stepper pins");
#endif

}


void Controller::configureGPIO(uint8_t pin, bool out, bool initialValue) {

    logger->trace("setting up stepper pin {}: direction: {}, initialValue: {}",
            pin,
            out ? "out" : "in",
            initialValue ? "on" : "off");
    // TODO: Pi pins
    //gpio_init(pin);
    //gpio_set_dir(pin, out);
    //gpio_put(pin, initialValue);

}

/*
void Controller::setCreatureWorkerTaskHandle(TaskHandle_t taskHandle) {
    this->creatureWorkerTaskHandle = taskHandle;
}
 */

void Controller::start() {
    logger->info("starting controller!");

    // TODO: Thread these

    /*
    // Fire off the housekeeper
    xTaskCreate(controller_housekeeper_task,
                "controller_housekeeper_task",
                256,
                (void *) this,
                1,
                &controllerHousekeeperTaskHandle);

    // Fire off the motor setup task
    xTaskCreate(controller_motor_setup_task,
                "controller_motor_setup_task",
                1024,
                (void *) this,
                1,
                &controller_motor_setup_task_handle);
    */

}

/**
 * Accepts input from an IOHandler
 *
 * @param input a buffer of size DMX_NUMBER_OF_CHANNELS containing the incoming data
 * @return true if it worked
 */
bool Controller::acceptInput(uint8_t *input) {

    // If we're not online, stop now and do nothing
    if(!online) {
        return false;
    }

    // Copy the incoming buffer into our buffer
    memcpy(currentFrame, input, numberOfChannels);

    /**
     * If there's no worker task, stop here.
     */
    //if (creatureWorkerTaskHandle == nullptr) {
    //    return false;
    //}

    // Send the processor a message
    // TODO Use a cv
    //xTaskNotify(creatureWorkerTaskHandle,
    //            0,
    //            eNoAction);

    return true;
}

uint8_t *Controller::getCurrentFrame() {
    return currentFrame;
}

void Controller::initServo(uint8_t indexNumber, const char *name, uint16_t minPulseUs,
                           uint16_t maxPulseUs, float smoothingValue, uint16_t defaultPosition, bool inverted) {

    uint8_t gpioPin = getPinMapping(indexNumber);

    /*
    servos[indexNumber] = new Servo(gpioPin, name, minPulseUs,
                                    maxPulseUs, smoothingValue, inverted,
                                    this->config->getServoFrequencyHz());
    */

    // Set it to its default position
    servos[indexNumber]->move(defaultPosition);

    numberOfServosInUse++;

    logger->info("controller servo init: index: {}, pin: {}, name: {}", indexNumber, gpioPin, name);

}

#if USE_STEPPERS
void Controller::initStepper(uint8_t slot, const char *name, uint32_t maxSteps, uint16_t decelerationAggressiveness,
                             uint32_t sleepWakeupPauseTimeUs, uint32_t sleepAfterUs, bool inverted) {

    steppers[slot] = new Stepper(logger, slot, name, maxSteps, decelerationAggressiveness, sleepWakeupPauseTimeUs,
                                 sleepAfterUs, inverted);
    numberOfSteppersInUse++;

    logger->info("controller stepper init: slot: {}, name: {}, max_steps: {}", slot, name, maxSteps);

}
#endif


uint8_t Controller::getPinMapping(uint8_t servoNumber) {
    return pinMappings[servoNumber];
}

uint16_t Controller::getServoPosition(uint8_t indexNumber) {
    return servos[indexNumber]->getPosition();
}

void Controller::requestServoPosition(uint8_t servoIndexNumber, uint16_t requestedPosition) {

    if (servos[servoIndexNumber]->getPosition() != requestedPosition) {
        logger->debug("requested to move servo {} from {} to position {}", servoIndexNumber,
              servos[servoIndexNumber]->getPosition(), requestedPosition);
        servos[servoIndexNumber]->move(requestedPosition);

    }
}

#if USE_STEPPERS
uint32_t Controller::getStepperPosition(uint8_t indexNumber) {
    return steppers[indexNumber]->state->currentMicrostep / STEPPER_MICROSTEP_MAX;
}

/**
 * Request a position in full steps
 *
 * This will be converted to microsteps transparently!
 *
 * @param stepperIndexNumber stepper to modify
 * @param requestedPosition the number of full steps to request
 */
void Controller::requestStepperPosition(uint8_t stepperIndexNumber, uint32_t requestedPosition) {

    if (steppers[stepperIndexNumber]->state->requestedSteps != requestedPosition) {

        logger->debug("requested to move stepper {} from {} to position {}", stepperIndexNumber,
              steppers[stepperIndexNumber]->state->requestedSteps, requestedPosition);

        steppers[stepperIndexNumber]->state->requestedSteps = requestedPosition;
        steppers[stepperIndexNumber]->state->moveRequested = true;
    }
}
#endif

/*
void __isr Controller::on_pwm_wrap_handler() {

    for (int i = 0; i < numberOfServosInUse; i++)

        pwm_set_chan_level(servos[i]->getSlice(),
                           servos[i]->getChannel(),
                           servos[i]->getCurrentTick());

    pwm_clear_irq(servos[0]->getSlice());

    Controller::numberOfPWMWraps++;


    //If there's no worker task, stop here.
    if (controllerHousekeeperTaskHandle != nullptr) {

        // Tell the housekeeper to go when it can
        xTaskNotifyFromISR(controllerHousekeeperTaskHandle,
                           0,
                           eNoAction,
                           &isrPriorityTaskWoken);
    }
}
*/

uint32_t Controller::getNumberOfPWMWraps() {
    return numberOfPWMWraps;
}


bool Controller::hasReceivedFirstFrame() {
    return receivedFirstFrame;
}


uint8_t Controller::getNumberOfServosInUse() {
    return numberOfServosInUse;
}

uint16_t Controller::getNumberOfDMXChannels() {
    return numberOfChannels;
}

void Controller::setOnline(bool onlineValue) {
    logger->info("setting online to{}", onlineValue ? "true" : "false");
    this->online = onlineValue;
}

bool Controller::isOnline() {
    return online;
}

Servo *Controller::getServo(uint8_t index) {
    return servos[index];
}

#if USE_STEPPERS
uint8_t Controller::getNumberOfSteppersInUse() {
    return numberOfSteppersInUse;
}

Stepper *Controller::getStepper(uint8_t index) {
    return steppers[index];
}
#endif

/*
portTASK_FUNCTION(controller_housekeeper_task, pvParameters) {
auto controller = (Controller *) pvParameters;

uint32_t ulNotifiedValue;

debug("controller housekeeper running");

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

for (EVER) {

// Wait for the ISR to signal us to go
xTaskNotifyWait(0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);

for (int i = 0; i < controller->getNumberOfServosInUse(); i++) {

// Do housekeeping on each servo
controller->getServo(i)->calculateNextTick();

}
}
#pragma clang diagnostic pop
}
*/


/**
 * Motor setup task
 *
 * This is in a task so that we have access to the full functionality of the
 * Creature Controller while doing this process. (Logging, debug shell, etc.)
 *
 * When it's done it will start ISRs and repeating tasks needed for the
 * controller to actually function, and then terminate itself.
 *
 * @param pvParameters
 */
/*
portTASK_FUNCTION(controller_motor_setup_task, pvParameters) {

auto controller = (Controller *) pvParameters;

info("---> controller motor setup running");

// Install the IRQ handler for the servos
pwm_set_irq_enabled(controller->getServo(0)->getSlice(), true);
irq_set_exclusive_handler(PWM_IRQ_WRAP, Controller::on_pwm_wrap_handler);
irq_set_enabled(PWM_IRQ_WRAP, true);

#if USE_STEPPERS
// Set up the stepper timer
home_stepper(0);
controller->getStepper(0)->state->currentMicrostep = controller->getStepper(0)->maxMicrosteps;
add_repeating_timer_us(STEPPER_LOOP_PERIOD_IN_US, stepper_timer_handler, nullptr, &stepper_timer);
#endif

info("stopping the motor setup task");
vTaskDelete(nullptr);

}
 */