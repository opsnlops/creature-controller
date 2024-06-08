
#pragma once

#include <unordered_map>
#include <vector>

#include "config/UARTDevice.h"
#include "io/Message.h"
#include "logging/Logger.h"
#include "util/MessageQueue.h"
#include "util/Result.h"
#include "util/StoppableThread.h"


/**
 * There's a big circular dependency here. Forward declarations won't work because we need to
 * be able to call actual functions from the classes.
 *
 * Instead, let's stick to just what we _actually_ need, the message queue. It's one of the most
 * basic types in the system, and we can use it to break the dependency loop.
 */


namespace creatures :: io {

    /**
     * This class is a message router! It handles messages going out to the creature
     * and back in from it.
     */
    class MessageRouter : public StoppableThread {
    public:

        explicit MessageRouter(const std::shared_ptr<Logger>& logger);

        ~MessageRouter() override  {
            this->logger->info("MessageRouter destroyed");
        }

        /**
         * Register a ServoModuleHandler handler with the message router
         *
         * @param moduleName the name of the module to register
         * @param incomingQueue the incoming queue for the module
         */
        void registerServoModuleHandler(creatures::config::UARTDevice::module_name moduleName,
                                        std::shared_ptr<MessageQueue<Message>> incomingQueue);

        /**
         * Send a message to the creature
         *
         * It will be routed to the appropriate serial handler based on the `message`.
         *
         * @param message the message to route
         * @return a `Result` indicating success or failure
         */
        Result<bool> sendMessageToCreature(const Message &message);

        /**
         * Broadcast a message to all creatures
         *
         * @param message the message to broadcast
         */
        void broadcastMessageToAllModules(const std::string &message);

        /**
         * Received a message from the creature
         *
         * @param message the inbound message from the creature
         * @throws UnknownMessageDestinationException if the message has an un-routable destination
         */
        void receivedMessageFromCreature(const Message &message);

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

        // A big 'ole map of module names to their incoming queues
        std::unordered_map<creatures::config::UARTDevice::module_name, std::shared_ptr<MessageQueue<Message>>> moduleQueues;

    };
}