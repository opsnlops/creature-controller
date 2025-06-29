
#pragma once

#include <unordered_map>
#include <vector>

#include "config/UARTDevice.h"
#include "io/Message.h"
#include "logging/Logger.h"
#include "util/MessageQueue.h"
#include "util/Result.h"
#include "util/StoppableThread.h"

namespace creatures :: io {

    enum MotorHandlerState {
        unknown,
        idle,
        awaitingConfiguration,
        configuring,
        ready,
        running,
        stopped
    };

    /**
     * Routes messages between the controller and servo modules
     *
     * This class follows a simple philosophy: route messages where they need to go,
     * and if anything goes wrong, log it and continue. No complex recovery attempts.
     * Keep the message flow simple, like a well-organized rabbit warren! 🐰
     */
    class MessageRouter : public StoppableThread {
    public:

        explicit MessageRouter(const std::shared_ptr<Logger>& logger);

        ~MessageRouter() override  {
            this->logger->info("MessageRouter destroyed");
        }

        /**
         * Register a ServoModuleHandler with the message router
         *
         * @param moduleName the name of the module to register
         * @param incomingMessages the incoming message queue for the handler
         * @param outgoingMessages the outgoing message queue for the handler
         * @return a `Result` indicating success or failure
         */
        Result<bool> registerServoModuleHandler(creatures::config::UARTDevice::module_name moduleName,
                                        std::shared_ptr<MessageQueue<Message>> incomingMessages,
                                        std::shared_ptr<MessageQueue<Message>> outgoingMessages);

        /**
         * Send a message to a specific creature module
         *
         * @param message the message to route
         * @return a `Result` indicating success or failure
         */
        Result<bool> sendMessageToCreature(const Message &message);

        /**
         * Broadcast a message to all registered modules
         *
         * @param message the message to broadcast
         */
        void broadcastMessageToAllModules(const std::string &message);

        /**
         * Receive a message from a creature module
         *
         * @param message the inbound message from the creature
         */
        Result<bool> receivedMessageFromCreature(const Message &message);

        // For the controller to read messages from creatures
        std::shared_ptr<MessageQueue<Message>> getIncomingQueue();

        void start() override;

        /**
         * Check if all handlers are ready
         *
         * @return true if all registered handlers are ready
         */
        bool allHandlersReady();

        /**
         * Set the state of a handler
         *
         * @param moduleName the name of the module to set the state for
         * @param state the state to set
         * @return true if the state was set, false otherwise
         */
        Result<bool> setHandlerState(creatures::config::UARTDevice::module_name moduleName, MotorHandlerState state);

        /**
         * Get all registered handler IDs
         *
         * @return a vector of module names
         */
        std::vector<creatures::config::UARTDevice::module_name> getHandleIds();

    protected:
        void run() override;

    private:

        // Internal struct to hold the incoming and outgoing queues for a handler
        struct HandlerQueues {
            std::shared_ptr<MessageQueue<Message>> incomingQueue;
            std::shared_ptr<MessageQueue<Message>> outgoingQueue;
        };

        std::shared_ptr<Logger> logger;

        // Messages in from creatures
        std::shared_ptr<MessageQueue<Message>> incomingQueue;

        // Map of module names to their message queues
        std::unordered_map<creatures::config::UARTDevice::module_name, HandlerQueues> servoHandlers;

        // Track the state of each handler
        std::unordered_map<creatures::config::UARTDevice::module_name, MotorHandlerState> handlerStates;
    };

}