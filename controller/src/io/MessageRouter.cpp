
#include <string>

#include "config/UARTDevice.h"
#include "io/Message.h"
#include "io/MessageRouter.h"
#include "io/SerialHandler.h"
#include "io/UnknownMessageDestintationException.h"
#include "logging/Logger.h"


namespace creatures :: io {

    using creatures::config::UARTDevice;
    using creatures::io::Message;
    using creatures::SerialHandler;
    using creatures::UnknownMessageDestinationException;

    MessageRouter::MessageRouter(const std::shared_ptr<Logger>& logger) : logger(logger) {

        // Set up our message queues
        this->incomingQueue = std::make_shared<MessageQueue<Message>>();
        this->outgoingQueue = std::make_shared<MessageQueue<Message>>();

        this->logger->info("MessageRouter created");
    }

    void MessageRouter::registerServoModuleHandler(creatures::config::UARTDevice::module_name moduleName,
                                    std::shared_ptr<MessageQueue<Message>> _incomingQueue) {
        this->moduleQueues[moduleName] = _incomingQueue;
        this->logger->info("Registered module: {}", UARTDevice::moduleNameToString(moduleName));
    }

    void MessageRouter::sendMessageToCreature(const Message &message) {

        logger->trace("Sending message to creature on module {}: {}",
                      UARTDevice::moduleNameToString(message.module),
                      message.payload);

        for (const auto& pair : moduleQueues) {
            creatures::config::UARTDevice::module_name key = pair.first;
            std::shared_ptr<MessageQueue<Message>> queue = pair.second;

            // Deliver the message to the appropriate module
            if(message.module == key) {
                queue->push(message);
                return;
            }

            // If we get here, that's bad
            std::string errorMessage = fmt::format("Unknown destination into the message router: {}",
                                                   UARTDevice::moduleNameToString(message.module));
            this->logger->critical(errorMessage);
            throw UnknownMessageDestinationException(errorMessage);
        }


    }

    void MessageRouter::broadcastMessageToAllModules(const std::string &message) {

        logger->info("📣 Broadcasting message to all modules: {}", message);

        for (const auto& pair : moduleQueues) {
            creatures::config::UARTDevice::module_name key = pair.first;
            std::shared_ptr<MessageQueue<Message>> queue = pair.second;

            // Deliver this message in all it's glory
            queue->push(Message(key, message));
        }
    }

    void MessageRouter::receivedMessageFromCreature(const Message &message) {
            incomingQueue->push(message);
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
        }
    }

}