

#include <exception>
#include <string>
#include <vector>


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

    void MessageRouter::registerSerialHandler(const std::shared_ptr<SerialHandler>& serialHandler) {
        this->serialHandlers.push_back(serialHandler);

        logger->info("Registered serial handler for module: {}",
                     UARTDevice::moduleNameToString(serialHandler->getModuleName()));

    }

    void MessageRouter::sendMessageToCreature(const Message &message) {

        logger->trace("Sending message to creature on module {}: {}",
                      UARTDevice::moduleNameToString(message.module),
                      message.payload);

        // Go looking for which module to send this message to
        for (const auto& handler : this->serialHandlers) {
            if (handler->getModuleName() == message.module) {
                handler->getOutgoingQueue()->push(message);
                return;
            }
        }

        // If we get here, we tried to send a message to a module we don't know about, so abort! ⛔️
        std::string errorMessage = fmt::format("Unknown destination: {}",
                                               UARTDevice::moduleNameToString(message.module));
        logger->critical(errorMessage);
        throw UnknownMessageDestinationException(errorMessage);
    }

    void MessageRouter::broadcastMessageToAllModules(const std::string &message) {

        logger->info("📣 Broadcasting message to all modules: {}", message);

        for (const auto& handler : this->serialHandlers) {
            handler->getOutgoingQueue()->push(Message(handler->getModuleName(), message));
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