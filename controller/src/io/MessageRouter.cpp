#include <string>

#include "config/UARTDevice.h"
#include "io/Message.h"
#include "io/MessageRouter.h"
#include "logging/Logger.h"
#include "util/thread_name.h"

namespace creatures ::io {

using creatures::config::UARTDevice;
using creatures::io::Message;

MessageRouter::MessageRouter(const std::shared_ptr<Logger> &logger) : logger(logger) {

    // Set up our message queues
    this->incomingQueue = std::make_shared<MessageQueue<Message>>();

    // Initialize our maps
    this->servoHandlers = std::unordered_map<creatures::config::UARTDevice::module_name, HandlerQueues>();
    this->handlerStates = std::unordered_map<creatures::config::UARTDevice::module_name, MotorHandlerState>();

    this->threadName = "MessageRouter::run";
    this->logger->info("MessageRouter created - ready for message routing");
}

Result<bool> MessageRouter::registerServoModuleHandler(creatures::config::UARTDevice::module_name moduleName,
                                                       std::shared_ptr<MessageQueue<Message>> incomingMessages,
                                                       std::shared_ptr<MessageQueue<Message>> outgoingMessages) {

    // Make sure this module isn't already registered
    if (this->servoHandlers.find(moduleName) != this->servoHandlers.end()) {
        std::string errorMessage =
            fmt::format("Module {} is already registered", UARTDevice::moduleNameToString(moduleName));
        this->logger->error(errorMessage);
        return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
    }

    this->servoHandlers[moduleName] = {incomingMessages, outgoingMessages};
    this->handlerStates[moduleName] = MotorHandlerState::unknown;
    this->logger->info("Registered module: {}", UARTDevice::moduleNameToString(moduleName));
    return Result<bool>{true};
}

Result<bool> MessageRouter::sendMessageToCreature(const Message &message) {

    logger->trace("Sending message to creature on module {}: {}", UARTDevice::moduleNameToString(message.module),
                  message.payload);

    // Find the handler for this module
    auto it = servoHandlers.find(message.module);
    if (it != servoHandlers.end()) {
        // Found the handler, send the message
        it->second.outgoingQueue->push(message);
        return Result<bool>{true};
    }

    // Module not found - this is an error
    std::string errorMessage =
        fmt::format("Unknown destination module: {}", UARTDevice::moduleNameToString(message.module));
    this->logger->error(errorMessage);
    return Result<bool>{ControllerError(ControllerError::DestinationUnknown, errorMessage)};
}

void MessageRouter::broadcastMessageToAllModules(const std::string &message) {

    logger->info("📣 Broadcasting message to all modules: {}", message);

    for (const auto &pair : servoHandlers) {
        creatures::config::UARTDevice::module_name moduleName = pair.first;
        auto handlerQueues = pair.second;

        // Create a message for this module and send it
        handlerQueues.outgoingQueue->push(Message(moduleName, message));
    }
}

Result<bool> MessageRouter::receivedMessageFromCreature(const Message &message) {
    incomingQueue->push(message);
    return Result<bool>{true};
}

std::shared_ptr<MessageQueue<Message>> MessageRouter::getIncomingQueue() { return this->incomingQueue; }

void MessageRouter::start() {
    this->logger->info("starting the message router");
    creatures::StoppableThread::start();
}

void MessageRouter::shutdown() {
    this->logger->info("shutting down MessageRouter");

    // Signal the incoming queue to shutdown to wake up any blocked threads
    if (incomingQueue) {
        incomingQueue->request_shutdown();
    }

    // Call parent shutdown to stop the thread
    creatures::StoppableThread::shutdown();
}

void MessageRouter::run() {
    setThreadName(threadName);
    this->logger->info("MessageRouter running");

    while (!this->stop_requested.load()) {

        // Wait for a message to come in from one of our modules with a timeout
        // so we can check for shutdown requests regularly - like a rabbit with alert ears! 🐰
        auto messageOpt = this->incomingQueue->pop_timeout(std::chrono::milliseconds(100));

        if (messageOpt.has_value()) {
            auto incomingMessage = messageOpt.value();
            this->logger->debug("incoming message from a creature: {}", incomingMessage.payload);

            // TODO: Something maybe should happen here other than logging it
        }
        // If no message arrived, we just continue the loop to check stop_requested
    }

    this->logger->debug("MessageRouter stopped!");
}

Result<bool> MessageRouter::setHandlerState(creatures::config::UARTDevice::module_name moduleName,
                                            MotorHandlerState state) {

    // Make sure this handler is registered
    if (handlerStates.find(moduleName) == handlerStates.end()) {
        std::string errorMessage =
            fmt::format("Module {} is not registered", UARTDevice::moduleNameToString(moduleName));
        this->logger->error(errorMessage);
        return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
    }

    this->handlerStates[moduleName] = state;
    this->logger->debug("Set module {} state to {}", UARTDevice::moduleNameToString(moduleName),
                        static_cast<int>(state));
    return Result<bool>{true};
}

bool MessageRouter::allHandlersReady() {

    // Look at all the handlerStates and return false if any of them are not ready
    for (const auto &pair : handlerStates) {
        if (pair.second != MotorHandlerState::ready) {
            return false;
        }
    }

    return true;
}

std::vector<creatures::config::UARTDevice::module_name> MessageRouter::getHandleIds() {
    std::vector<creatures::config::UARTDevice::module_name> ids;
    ids.reserve(servoHandlers.size());
    for (const auto &pair : servoHandlers) {
        ids.push_back(pair.first);
    }
    return ids;
}

} // namespace creatures::io