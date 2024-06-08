

#include "controller-config.h"

#include "device/Servo.h"
#include "Controller.h"

#include "creature/Creature.h"
#include "config/UARTDevice.h"
#include "controller/Input.h"
#include "controller/CommandSendException.h"
#include "controller/commands/ICommand.h"
#include "controller/commands/ServoModuleConfiguration.h"
#include "controller/commands/FlushBuffer.h"
#include "controller/commands/SetServoPositions.h"
#include "io/Message.h"
#include "util/thread_name.h"

// Exceptions
#include "controller/ControllerException.h"


u64 number_of_moves = 0UL;


Controller::Controller(const std::shared_ptr<creatures::Logger>& logger,
                       const std::shared_ptr<creatures::creature::Creature>& creature,
                       std::shared_ptr<creatures::io::MessageRouter> messageRouter):
                       creature(creature), logger(logger), messageRouter(messageRouter) {

    logger->debug("setting up the controller");

    online = true;
    receivedFirstFrame = false;
    this->numberOfChannels = DMX_NUMBER_OF_CHANNELS;

    // Create our input queue
    inputQueue = std::make_shared<creatures::MessageQueue<std::unordered_map<std::string, creatures::Input>>>();
    logger->debug("created the input queue");



    logger->info("Controller for {} initialized", creature->getName());

}


void Controller::sendCommand(const std::shared_ptr<creatures::ICommand>& command,
                             creatures::config::UARTDevice::module_name destModule) {
    logger->trace("sending command {}", command->toMessageWithChecksum());


    //this->outgoingQueue->push(command->toMessageWithChecksum());
}


void Controller::start() {
    logger->info("starting controller!");
    creatures::StoppableThread::start();
}



bool Controller::acceptInput(const std::vector<creatures::Input>& inputs) {

    // Don't waste time with empty sets
    if(inputs.empty()) {
        logger->warn("ignoring an empty set of inputs");
        return false;
    }

    // The I/O handler cares about slots in the DMX stream, the creature
    // cares about names. Let's build the map the creature actually wants
    // here, so the creature doesn't have to do it.
    std::unordered_map<std::string, creatures::Input> creatureInputs;
    for(auto& input: inputs) {
        creatureInputs[input.getName()] = input;
    }

    // Is this first data we've gotten?
    if(!receivedFirstFrame) {
        logger->info("first frame received");
        receivedFirstFrame = true;
    }

    // Assign this to the input queue and hope the creature sees it!
    logger->trace("sending {} inputs to the input queue", creatureInputs.size());
    inputQueue->push(creatureInputs);

    return true;
}

void Controller::firmwareReadyForInitialization(u32 firmwareVersion) {

    // Make sure we got the version of the firmware we were built against
    if(firmwareVersion != FIRMWARE_VERSION) {
        std::string errorMessage = fmt::format("Firmware version mismatch! Expected {}, got {}",  FIRMWARE_VERSION, firmwareVersion);
        logger->critical(errorMessage);
        throw creatures::ControllerException(errorMessage);
    }

    logger->debug("firmware is ready for initialization (version {})", firmwareVersion);

    // Go gather the configuration from the creature
    auto creatureConfigCommand = creatures::commands::ServoModuleConfiguration(logger);
    creatureConfigCommand.getServoConfigurations(creature);

    // ...and toss it to the serial handler
#warning fix
    creatures::io::Message message = creatures::io::Message( creatures::config::UARTDevice::module_name::A, creatureConfigCommand.toMessageWithChecksum());

    auto sendResult = this->messageRouter->sendMessageToCreature(message);
    if(!sendResult.isSuccess()) {
        logger->critical("Failed to send the creature configuration to the firmware: {}", sendResult.getError()->getMessage());
    }

}

void Controller::sendFlushBufferRequest() {
    logger->info("Sending a request to the firmware to flush the buffer");

    // This is a special message that doesn't have a checksum. It's a magic character
    // that the firmware is looking for to know it's time to reset the buffer.
    auto flushBufferCommand = creatures::commands::FlushBuffer(logger);
    this->messageRouter->broadcastMessageToAllModules(flushBufferCommand.toMessage()); // No checksum, only 🔔
}


std::shared_ptr<creatures::MessageQueue<std::unordered_map<std::string, creatures::Input>>> Controller::getInputQueue() {
    return inputQueue;
}


std::shared_ptr<creatures::creature::Creature> Controller::getCreature() {
    return creature;
}



bool Controller::hasReceivedFirstFrame() {
    return receivedFirstFrame;
}

uint16_t Controller::getNumberOfDMXChannels() {
    return numberOfChannels;
}

void Controller::setOnline(bool onlineValue) {
    logger->info("setting online to {}", onlineValue ? "true" : "false");
    this->online = onlineValue;
}

bool Controller::isOnline() {
    return online;
}

void Controller::firmwareReadyToOperate() {
    logger->info("firmware is ready to operate");
    this->firmwareReady = true;
}




void Controller::run() {

    using namespace std::chrono;

    this->threadName = "Controller::run";
    setThreadName(this->threadName);

    logger->info("controller worker now running");




    auto target_delta = microseconds( 1000000 / creature->getServoUpdateFrequencyHz());
    auto next_target_time = high_resolution_clock::now() + target_delta;

    while (!stop_requested.load()) {

        number_of_frames = number_of_frames + 1;

        if(number_of_frames % 100 == 0) {
            u64 _frames = number_of_frames; // copy the atomic value
            logger->info("frames: {}", _frames);
        }

        // If we haven't received a frame yet, don't do anything
        if (receivedFirstFrame && firmwareReady) {

            // Go fetch the positions
#warning fix
            std::vector<creatures::ServoPosition> requestedPositions = creature->getRequestedServoPositions(creatures::config::UARTDevice::module_name::A);

            auto command = std::make_shared<creatures::commands::SetServoPositions>(logger);
            for (auto &position: requestedPositions) {
                command->addServoPosition(position);
            }

            // Fire this off to the controller
#warning fix
            sendCommand(command, creatures::config::UARTDevice::module_name::A);

            // Tell the creature to get ready for next time
            creature->calculateNextServoPositions();
        }
        else {

            // If we're stalled, log why every few frames
            if(number_of_frames % 100 == 0) {
                logger->warn("not sending frames because we're not ready! receivedFirstFrame: {}, firmwareReady: {}",
                             receivedFirstFrame, firmwareReady);

            }
        }

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
