#include <cstdlib>
#include <utility>

#include "config/UARTDevice.h"
#include "controller/ServoModuleHandler.h"
#include "controller/commands/ServoModuleConfiguration.h"
#include "io/Message.h"
#include "io/MessageProcessor.h"
#include "io/MessageRouter.h"
#include "io/SerialHandler.h"
#include "logging/Logger.h"
#include "util/MessageQueue.h"
#include "util/StoppableThread.h"
#include "util/thread_name.h"

namespace creatures {

using creatures::MessageProcessor;
using creatures::SerialHandler;
using creatures::config::UARTDevice;
using creatures::io::Message;
using creatures::io::MessageRouter;

ServoModuleHandler::ServoModuleHandler(
    std::shared_ptr<Logger> logger, std::shared_ptr<Controller> controller, UARTDevice::module_name moduleId,
    std::string deviceNode, std::shared_ptr<MessageRouter> messageRouter,
    std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue)
    : logger(logger), controller(controller), deviceNode(std::move(deviceNode)), moduleId(moduleId),
      messageRouter(messageRouter), websocketOutgoingQueue(websocketOutgoingQueue) {

    logger->info("creating a new ServoModuleHandler for module {} on node {}", UARTDevice::moduleNameToString(moduleId),
                 this->deviceNode);

    // Make our queues
    this->outgoingQueue = std::make_shared<MessageQueue<Message>>();
    this->incomingQueue = std::make_shared<MessageQueue<Message>>();

    this->messageRouter->setHandlerState(this->moduleId, creatures::io::MotorHandlerState::idle);

    this->threadName = fmt::format("ServoModuleHandler-{}", UARTDevice::moduleNameToString(this->moduleId));
}

void ServoModuleHandler::init() {
    // Don't initialize if we're shutting down
    if (is_shutting_down.load()) {
        logger->warn("Attempted to init ServoModuleHandler while shutting down");
        return;
    }

    logger->info("initializing the ServoModuleHandler for module {} on node {}",
                 UARTDevice::moduleNameToString(this->moduleId), this->deviceNode);

    // Make our message processor
    this->messageProcessor =
        std::make_unique<MessageProcessor>(logger, moduleId, shared_from_this(), websocketOutgoingQueue);

    // Create the SerialHandler
    this->serialHandler = std::make_shared<SerialHandler>(logger, this->deviceNode, this->moduleId, this->outgoingQueue,
                                                          this->incomingQueue);

    this->messageRouter->setHandlerState(this->moduleId, creatures::io::MotorHandlerState::awaitingConfiguration);
}

void ServoModuleHandler::shutdown() {
    logger->info("shutting down the ServoModuleHandler for module {} on node {}",
                 UARTDevice::moduleNameToString(this->moduleId), this->deviceNode);

    // Set shutdown flag to prevent new operations
    is_shutting_down.store(true);

    // Signal our own thread to stop
    stop_requested.store(true);

    // Request shutdown on queues to wake up blocked threads
    if (this->incomingQueue) {
        this->incomingQueue->request_shutdown();
        this->incomingQueue->clear();
    }

    if (this->outgoingQueue) {
        this->outgoingQueue->request_shutdown();
        this->outgoingQueue->clear();
    }

    // Tell the message router we've stopped
    this->messageRouter->setHandlerState(this->moduleId, creatures::io::MotorHandlerState::stopped);

    // IMPORTANT: Shut down serial handler FIRST to stop new messages coming in
    if (this->serialHandler) {
        logger->debug("Shutting down SerialHandler for module {}", UARTDevice::moduleNameToString(this->moduleId));
        this->serialHandler->shutdown();
    }

    // Shut down our message processor
    if (this->messageProcessor) {
        logger->debug("Shutting down MessageProcessor for module {}", UARTDevice::moduleNameToString(this->moduleId));
        this->messageProcessor->shutdown();
    }

    // Call parent shutdown to clean up our main thread
    logger->debug("Shutting down main thread for module {}", UARTDevice::moduleNameToString(this->moduleId));
    StoppableThread::shutdown();

    logger->debug("ServoModuleHandler shutdown complete for module {}", UARTDevice::moduleNameToString(this->moduleId));
}

void ServoModuleHandler::start() {
    // Don't start if we're shutting down
    if (is_shutting_down.load()) {
        logger->warn("Attempted to start ServoModuleHandler while shutting down");
        return;
    }

    if (this->messageProcessor) {
        this->messageProcessor->start();
    }

    if (this->serialHandler) {
        this->serialHandler->start();
    }

    creatures::StoppableThread::start();
}

std::shared_ptr<MessageQueue<Message>> ServoModuleHandler::getIncomingQueue() { return this->incomingQueue; }

std::shared_ptr<MessageQueue<Message>> ServoModuleHandler::getOutgoingQueue() { return this->outgoingQueue; }

Result<bool> ServoModuleHandler::firmwareReadyForInitialization(u32 firmwareVer) {
    // Don't process if we're shutting down
    if (is_shutting_down.load()) {
        return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, "Module is shutting down")};
    }

    // Accept any firmware version this controller knows how to talk to. A HW3
    // board reports 3 (standard servos only); a HW4 board reports 4 (adds
    // Dynamixel). One controller binary supports either.
    if (firmwareVer < MIN_SUPPORTED_FIRMWARE_VERSION || firmwareVer > MAX_SUPPORTED_FIRMWARE_VERSION) {
        std::string errorMessage =
            fmt::format("Unsupported firmware version on module {}: got {}, this controller supports {}-{}",
                        UARTDevice::moduleNameToString(getModuleName()), firmwareVer, MIN_SUPPORTED_FIRMWARE_VERSION,
                        MAX_SUPPORTED_FIRMWARE_VERSION);
        logger->critical(errorMessage);
        return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
    }

    // Save the firmware version
    this->firmwareVersion = firmwareVer;

    logger->info("firmware is ready for initialization on module {} (version {}, Dynamixel {})",
                 UARTDevice::moduleNameToString(getModuleName()), firmwareVer,
                 firmwareVer >= DYNAMIXEL_MIN_FIRMWARE_VERSION ? "supported" : "not supported");
    this->messageRouter->setHandlerState(this->moduleId, creatures::io::MotorHandlerState::configuring);

    // Go gather the configuration from the creature, gated to what this firmware
    // version can actually drive.
    auto creatureConfigCommand = creatures::commands::ServoModuleConfiguration(logger);
    auto configResult = creatureConfigCommand.getServoConfigurations(controller, getModuleName(), this->firmwareVersion);
    if (!configResult.isSuccess()) {
        // An incompatible creature/hardware pairing - e.g. a creature with
        // Dynamixel motors attached to HW3 firmware - cannot run on this board.
        // Surface it clearly and halt rather than limp along with a creature
        // that physically can't work here.
        logger->critical("FATAL: cannot configure module {} for this firmware: {}",
                         UARTDevice::moduleNameToString(getModuleName()), configResult.getError()->getMessage());
        logger->critical("Halting the controller. Check that the creature config matches the attached hardware.");
        std::exit(1);
    }

    // ...and toss it to the serial handler
    creatures::io::Message message =
        creatures::io::Message(getModuleName(), creatureConfigCommand.toMessageWithChecksum());

    auto sendResult = this->messageRouter->sendMessageToCreature(message);
    if (!sendResult.isSuccess()) {
        logger->critical("Failed to send the creature configuration to the firmware: {}",
                         sendResult.getError()->getMessage());
    }

    return Result<bool>{true};
}

void ServoModuleHandler::firmwareReadyToOperate() {
    // Don't process if we're shutting down
    if (is_shutting_down.load()) {
        logger->warn("Firmware ready signal received while shutting down");
        return;
    }

    logger->info("firmware is ready to operate");
    this->ready.store(true);
    this->configured.store(true);
    this->messageRouter->setHandlerState(this->moduleId, creatures::io::MotorHandlerState::ready);
}

Result<bool> ServoModuleHandler::sendMessageToController(std::string messagePayload) {
    // Don't process if we're shutting down
    if (is_shutting_down.load()) {
        return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, "Module is shutting down")};
    }

    logger->trace("sending message to controller: {}", messagePayload);

    auto message = Message(this->moduleId, messagePayload);

    auto result = this->messageRouter->receivedMessageFromCreature(message);
    if (!result.isSuccess()) {
        auto errorMessage =
            fmt::format("Failed to send message to the message router: {}", result.getError()->getMessage());
        logger->error(errorMessage);
        return result;
    }
    return Result<bool>{true};
}

void ServoModuleHandler::run() {
    setThreadName(threadName);

    logger->info("ServoModuleHandler thread started");

    while (!stop_requested.load()) {

        // Wait for a message to come in from one of our modules, but with a
        // timeout so we can hop out if we need to stop
        auto messageOpt = this->incomingQueue->pop_timeout(std::chrono::milliseconds(100));

        if (messageOpt.has_value()) {
            auto incomingMessage = messageOpt.value();
            this->logger->trace("incoming message: {}", incomingMessage.payload);

            // Go process it!
            messageProcessor->processMessage(incomingMessage);
        }
        // If no message arrived in the timeout period, we just continue the
        // loop This allows us to check stop_requested regularly - as responsive
        // as a rabbit's ears!
    }

    logger->info("ServoModuleHandler thread stopping");
}

creatures::config::UARTDevice::module_name ServoModuleHandler::getModuleName() const { return this->moduleId; }

bool ServoModuleHandler::isReady() const { return ready.load(); }

bool ServoModuleHandler::isConfigured() const { return configured.load(); }

} // namespace creatures