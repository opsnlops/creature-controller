
#include <string>

#include "config/UARTDevice.h"
#include "controller/ServoModuleHandler.h"
#include "io/Message.h"
#include "io/MessageRouter.h"
#include "io/SerialHandler.h"
#include "logging/Logger.h"


namespace creatures :: io {

    using creatures::config::UARTDevice;
    using creatures::io::Message;
    using creatures::SerialHandler;

    MessageRouter::MessageRouter(const std::shared_ptr<Logger>& logger) : logger(logger) {

        // Set up our message queues
        this->incomingQueue = std::make_shared<MessageQueue<Message>>();
        this->outgoingQueue = std::make_shared<MessageQueue<Message>>();

        // Create our map
        this->servoHandlers = std::unordered_map<creatures::config::UARTDevice::module_name,
                                                 HandlerQueues>();

        this->handlerStates = std::unordered_map<creatures::config::UARTDevice::module_name,
                                                 MotorHandlerState>();

        this->logger->info("MessageRouter created");
    }


    /*
     * The reason we're not accepting the ServoModuleHandler as a parameter is because it's a big circular
     * dependency. The ServoModuleHandler needs a MessageRouter, and the MessageRouter needs a ServoModuleHandler.
     *
     * So, instead, we're just sending the queues. That's what we really care about anyhow.
     */


    Result<bool> MessageRouter::registerServoModuleHandler(creatures::config::UARTDevice::module_name moduleName,
                                                           std::shared_ptr<MessageQueue<Message>> incomingMessages,
                                                           std::shared_ptr<MessageQueue<Message>> outgoingMessages) {

        // Make sure this module isn't already registered
        if(this->servoHandlers.find(moduleName) != this->servoHandlers.end()) {
            std::string errorMessage = fmt::format("Module {} is already registered",
                                                   UARTDevice::moduleNameToString(moduleName));
            this->logger->error(errorMessage);
            return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
        }

        this->servoHandlers[moduleName] = {incomingMessages, outgoingMessages};
        this->handlerStates[moduleName] = MotorHandlerState::unknown;
        this->logger->info("Registered module: {}", UARTDevice::moduleNameToString(moduleName));
        return Result<bool>{true};
    }

    Result<bool> MessageRouter::sendMessageToCreature(const Message &message) {

        logger->trace("Sending message to creature on module {}: {}",
                      UARTDevice::moduleNameToString(message.module),
                      message.payload);

        for (const auto& pair : servoHandlers) {
            creatures::config::UARTDevice::module_name key = pair.first;
            auto handlerQueues = pair.second;

            // Deliver the message to the appropriate module
            if (message.module == key) {
                handlerQueues.outgoingQueue->push(message);
                return Result<bool>{true};
            }
        }

        // If we get here, that's bad
        std::string errorMessage = fmt::format("Unknown destination into the message router: {}",
                                               UARTDevice::moduleNameToString(message.module));
        this->logger->critical(errorMessage);
        return Result<bool>{ControllerError(ControllerError::DestinationUnknown, errorMessage)};

    }

    void MessageRouter::broadcastMessageToAllModules(const std::string &message) {

        logger->info("📣 Broadcasting message to all modules: {}", message);

        for (const auto& pair : servoHandlers) {
            creatures::config::UARTDevice::module_name key = pair.first;
            auto handlerQueues = pair.second;

            // Deliver this message in all it's glory
            handlerQueues.outgoingQueue->push(Message(key, message));
        }
    }

    Result<bool> MessageRouter::receivedMessageFromCreature(const Message &message) {
            incomingQueue->push(message);
            return Result<bool>{true};
    }

    std::shared_ptr<MessageQueue<Message>> MessageRouter::getIncomingQueue() {
        return this->incomingQueue;
    }

    void MessageRouter::start() {
        this->logger->info("starting the message router");
        creatures::StoppableThread::start();
    }

    void MessageRouter::run() {
        this->logger->info("MessageRouter running");

        while(!stop_requested.load()) {

            // Wait for a message to come in from one of our modules
            //  TODO: Something maybe should happen here other than logging it
            auto incomingMessage = this->incomingQueue->pop();
            this->logger->debug("incoming message from a creature: {}", incomingMessage.payload);
        }

        this->logger->debug("MessageRouter stopped!");
    }

    Result<bool> MessageRouter::setHandlerState(creatures::config::UARTDevice::module_name moduleName, MotorHandlerState state) {

        // Make sure this handler is registered
        if(handlerStates.find(moduleName) == handlerStates.end()) {
            std::string errorMessage = fmt::format("Module {} is not registered",
                                                   UARTDevice::moduleNameToString(moduleName));
            this->logger->error(errorMessage);
            return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
        }

        this->handlerStates[moduleName] = state;
        return Result<bool>{true};
    }

    bool MessageRouter::allHandlersReady() {

        // Look at all the handlerStates and return false if any of them are not ready
        for (const auto& pair : handlerStates) {
            if(pair.second != MotorHandlerState::ready) {
                return false;
            }
        }

        return true;
    }

    std::vector<creatures::config::UARTDevice::module_name> MessageRouter::getHandleIds() {
        std::vector<creatures::config::UARTDevice::module_name> ids;
        for (const auto& pair : servoHandlers) {
            ids.push_back(pair.first);
        }
        return ids;
    }


}