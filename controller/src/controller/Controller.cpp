

#include <climits>
#include <utility>

#include "controller-config.h"

#include "device/Servo.h"
#include "Controller.h"

#include "creature/Creature.h"
#include "controller/CommandSendException.h"
#include "controller/commands/ICommand.h"
#include "controller/commands/SetServoPositions.h"

u64 number_of_moves = 0UL;

// TODO: Threads now
//extern TaskHandle_t controllerHousekeeperTaskHandle;
//extern TaskHandle_t controller_motor_setup_task_handle;
//BaseType_t isrPriorityTaskWoken = pdFALSE;


Controller::Controller(std::shared_ptr<creatures::Logger> logger): logger(logger) {

    logger->debug("setting up the controller");

    //creatureWorkerTaskHandle = nullptr;
    online = true;
    receivedFirstFrame = false;

}

void Controller::init(std::shared_ptr<Creature> creature, std::shared_ptr<creatures::SerialHandler> serialHandler) {

    this->creature = creature;
    this->serialHandler = serialHandler;
    this->numberOfChannels = DMX_NUMBER_OF_CHANNELS;

    logger->info("Controller for {} initialized", creature->getName());

}


void Controller::sendCommand(const std::shared_ptr<creatures::ICommand>& command) {
    logger->trace("sending command {}", command->toMessageWithChecksum());
    serialHandler->getOutgoingQueue()->push(command->toMessageWithChecksum());
}


void Controller::start() {
    logger->info("starting controller!");

    // Start the worker
    workerThread = std::thread(&Controller::worker, this);
    workerThread.detach();
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

std::shared_ptr<Creature> Controller::getCreature() {
    return creature;
}



bool Controller::hasReceivedFirstFrame() {
    return receivedFirstFrame;
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

u64 Controller::getNumberOfFrames() {
    return number_of_frames;
}

void Controller::worker() {

    using namespace std::chrono;

    logger->info("controller worker now running");

    // Mark ourselves as running
    workerRunning = true;

    auto target_delta = microseconds( 1000000 / creature->getServoUpdateFrequencyHz());
    auto next_target_time = high_resolution_clock::now() + target_delta;

    while (workerRunning) {

        number_of_frames = number_of_frames + 1;

        if(number_of_frames % 100 == 0) {
            logger->info("frames: {}", number_of_frames);
        }

        // Go fetch the positions
        std::vector<creatures::ServoPosition> requestedPositions = creature->getRequestedServoPositions();

        auto command = std::make_shared<creatures::commands::SetServoPositions>(logger);
        for(auto& position : requestedPositions) {
            command->addServoPosition(position);
        }

        // Fire this off to the controller
        sendCommand(command);

        // Tell the creature to get ready for next time
        creature->calculateNextServoPositions();

        // Figure out how much time we have until the next tick
        auto remaining_time = next_target_time - high_resolution_clock::now();

        // If there's time left, wait.
        if (remaining_time > nanoseconds(0)) {
            // Sleep for the remaining time
            std::this_thread::sleep_for(remaining_time);
        }

        // Update the target time for the next iteration
        next_target_time += target_delta;

    }

    logger->info("controller worker stopped");

}

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