

#include "controller-config.h"

#include "Controller.h"
#include "device/Servo.h"

#include "config/UARTDevice.h"
#include "controller/CommandSendException.h"
#include "controller/Input.h"
#include "controller/commands/FlushBuffer.h"
#include "controller/commands/ICommand.h"
#include "controller/commands/ServoModuleConfiguration.h"
#include "controller/commands/SetServoPositions.h"
#include "creature/Creature.h"
#include "io/Message.h"
#include "util/thread_name.h"

u64 number_of_moves = 0UL;

Controller::Controller(const std::shared_ptr<creatures::Logger> &logger,
                       const std::shared_ptr<creatures::creature::Creature> &creature,
                       std::shared_ptr<creatures::io::MessageRouter> messageRouter)
    : creature(creature), logger(logger), messageRouter(messageRouter) {

    logger->debug("setting up the controller");

    online = true;
    receivedFirstFrame = false;
    this->numberOfChannels = DMX_NUMBER_OF_CHANNELS;

    // Create our input queue
    inputQueue = std::make_shared<creatures::MessageQueue<std::unordered_map<std::string, creatures::Input>>>();
    logger->debug("created the input queue");

    logger->info("Controller for {} initialized", creature->getName());
}

void Controller::sendCommand(const std::shared_ptr<creatures::ICommand> &command,
                             creatures::config::UARTDevice::module_name destModule) {
    logger->trace("sending command {}", command->toMessageWithChecksum());

    this->messageRouter->sendMessageToCreature(creatures::io::Message(destModule, command->toMessageWithChecksum()));
}

void Controller::start() {
    logger->info("starting controller!");
    creatures::StoppableThread::start();
}

bool Controller::acceptInput(const std::vector<creatures::Input> &inputs) {

    // Don't waste time with empty sets
    if (inputs.empty()) {
        logger->warn("ignoring an empty set of inputs");
        return false;
    }

    // The I/O handler cares about slots in the DMX stream, the creature
    // cares about names. Let's build the map the creature actually wants
    // here, so the creature doesn't have to do it.
    std::unordered_map<std::string, creatures::Input> creatureInputs;
    for (auto &input : inputs) {
        creatureInputs[input.getName()] = input;
    }

    // Is this first data we've gotten?
    if (!receivedFirstFrame) {
        logger->info("first frame received");
        receivedFirstFrame = true;
    }

    // Assign this to the input queue and hope the creature sees it!
    logger->trace("sending {} inputs to the input queue", creatureInputs.size());
    inputQueue->push(creatureInputs);

    return true;
}

void Controller::sendFlushBufferRequest() {
    logger->info("Sending a request to the firmware to flush the buffer");

    // This is a special message that doesn't have a checksum. It's a magic
    // character that the firmware is looking for to know it's time to reset the
    // buffer.
    auto flushBufferCommand = creatures::commands::FlushBuffer(logger);
    this->messageRouter->broadcastMessageToAllModules(flushBufferCommand.toMessage()); // No checksum, only 🔔
}

creatures::Result<std::vector<creatures::ServoConfig>>
Controller::getServoConfigs(creatures::config::UARTDevice::module_name module) {

    auto configs = creature->getServoConfigs(module);
    if (configs.empty()) {
        auto errorMessage = fmt::format("no servo configurations found for module {}",
                                        creatures::config::UARTDevice::moduleNameToString(module));
        logger->error(errorMessage);
        return creatures::Result<std::vector<creatures::ServoConfig>>{
            creatures::ControllerError(creatures::ControllerError::InvalidConfiguration, errorMessage)};
    }

    return creatures::Result<std::vector<creatures::ServoConfig>>{configs};
}

std::shared_ptr<creatures::MessageQueue<std::unordered_map<std::string, creatures::Input>>>
Controller::getInputQueue() {
    return inputQueue;
}

std::shared_ptr<creatures::creature::Creature> Controller::getCreature() { return creature; }

bool Controller::hasReceivedFirstFrame() const { return receivedFirstFrame; }

uint16_t Controller::getNumberOfDMXChannels() const { return numberOfChannels; }

void Controller::setOnline(bool onlineValue) {
    logger->info("setting online to {}", onlineValue ? "true" : "false");
    this->online = onlineValue;
}

bool Controller::isOnline() { return online; }

void Controller::run() {

    using namespace std::chrono;

    this->threadName = "Controller::run";
    setThreadName(this->threadName);

    logger->info("controller worker now running");

    auto target_delta = microseconds(1000000 / creature->getServoUpdateFrequencyHz());
    auto next_target_time = high_resolution_clock::now() + target_delta;

    // State for the periodic summary. Tracking the wall clock lets us report the
    // rate we actually achieved, which says far more about the health of the
    // loop than a frame count that only ever goes up.
    auto lastSummaryTime = steady_clock::now();
    u64 lastSummaryFrames = 0;
    bool wasReady = true;

    while (!stop_requested.load()) {

        number_of_frames = number_of_frames + 1;

        // Fine-grained progress, for when you are watching a problem happen
        if (number_of_frames % CONTROLLER_FRAME_LOG_INTERVAL == 0) {
            u64 _frames = number_of_frames; // copy the atomic value
            logger->debug("frames: {}", _frames);
        }

        if (number_of_frames % CONTROLLER_FRAME_SUMMARY_INTERVAL == 0) {
            const u64 _frames = number_of_frames;
            const auto now = steady_clock::now();
            const auto elapsed = duration_cast<milliseconds>(now - lastSummaryTime).count();

            if (elapsed > 0) {
                const double fps = static_cast<double>(_frames - lastSummaryFrames) * 1000.0 / elapsed;
                logger->info("frames: {} ({:.1f} fps)", _frames, fps);
            } else {
                logger->info("frames: {}", _frames);
            }

            lastSummaryTime = now;
            lastSummaryFrames = _frames;
        }

        // If we haven't received a frame yet, don't do anything
        const bool ready = receivedFirstFrame && messageRouter->allHandlersReady();

        // Announce the transition either way. Being stalled is worth saying
        // once and worth repeating occasionally, but not every couple of
        // seconds for as long as it lasts.
        if (ready != wasReady) {
            if (ready) {
                logger->info("sending frames now: receivedFirstFrame and all handlers are ready");
            } else {
                logger->warn("not sending frames because we're not ready! "
                             "receivedFirstFrame: {}, firmwareReady: {}",
                             receivedFirstFrame, messageRouter->allHandlersReady());
            }
            wasReady = ready;
        }

        if (ready) {

            // Go fetch the positions

            // Look at each handler in the message router
            for (auto &handlerId : messageRouter->getHandleIds()) {

                std::vector<creatures::ServoPosition> requestedPositions =
                    creature->getRequestedServoPositions(handlerId);

                auto command = std::make_shared<creatures::commands::SetServoPositions>(logger);
                for (auto &position : requestedPositions) {
                    command->addServoPosition(position);
                }

                // Fire this off to the controller
                sendCommand(command, handlerId);
            }

            // Tell the creature to get ready for next time
            creature->calculateNextServoPositions();
        } else {

            // Still stalled - remind us at the summary cadence rather than
            // every couple of seconds, so a long stall stays visible without
            // burying everything else
            if (number_of_frames % CONTROLLER_FRAME_SUMMARY_INTERVAL == 0) {
                logger->warn("still not sending frames! "
                             "receivedFirstFrame: {}, firmwareReady: {}",
                             receivedFirstFrame, messageRouter->allHandlersReady());
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
