
#pragma once

#include <vector>

#include "config/UARTDevice.h"
#include "io/Message.h"
#include "io/SerialHandler.h"
#include "logging/Logger.h"
#include "util/MessageQueue.h"
#include "util/StoppableThread.h"


namespace creatures :: io {

    using creatures::config::UARTDevice;
    using creatures::io::Message;
    using creatures::SerialHandler;


    /**
     * This class is a message router! It handles messages going out to the creature
     * and back in from it.
     */
    class MessageRouter : public StoppableThread {
    public:

        MessageRouter(const std::shared_ptr<Logger>& logger);

        ~MessageRouter() override {
            this->logger->info("MessageRouter destroyed");
        }

        /**
         * Register a serial handler with the message router
         *
         * @param serialHandler
         */
        void registerSerialHandler(const std::shared_ptr<SerialHandler>& serialHandler);

        /**
         * Send a message to the creature
         *
         * It will be routed to the appropriate serial handler based on the `message`.
         *
         * @param message the message to route
         * @throws UnknownMessageDestinationException if the message has an unroutable destination
         */
        void sendMessageToCreature(const Message &message);

        /**
         * Broadcast a message to all creatures
         *
         * @param message the message to broadcast
         */
        void broadcastMessageToAllModules(const std::string &message);

        /**
         * Receive a message from the creature
         *
         * @param message the inbound message from the creature
         * @throws UnknownMessageDestinationException if the message has an unroutable destination
         */
        void receiveMessageFromCreature(const Message &message);

        // For the controller to read messages from the creature
        std::shared_ptr<MessageQueue<Message>> getIncomingQueue();

        void start() override;

    protected:
        void run() override;

    private:
        std::shared_ptr<Logger> logger;

        // Messages in from the creature
        std::shared_ptr<MessageQueue<Message>> incomingQueue;

        // Messages out to the creature
        std::shared_ptr<MessageQueue<Message>> outgoingQueue;

        std::vector<std::shared_ptr<SerialHandler>> serialHandlers;

    };
}