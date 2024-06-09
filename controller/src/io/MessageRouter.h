
#pragma once

#include <unordered_map>
#include <vector>

#include "config/UARTDevice.h"
#include "controller/ServoModuleHandler.h"
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
         * @param incomingMessages the incoming message queue for the handler
         * @param outgoingMessages the outgoing message queue for the handler
         *
         * The result will be stored in an internal map for routing messages that contains
         * the module name as the key and a struct containing the incoming and outgoing queues.
         *
         * @return a `Result` indicating success or failure
         */
        Result<bool> registerServoModuleHandler(creatures::config::UARTDevice::module_name moduleName,
                                        std::shared_ptr<MessageQueue<Message>> incomingMessages,
                                        std::shared_ptr<MessageQueue<Message>> outgoingMessages);

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
         */
        Result<bool> receivedMessageFromCreature(const Message &message);

        // For the controller to read messages from the creature
        std::shared_ptr<MessageQueue<Message>> getIncomingQueue();

        void start() override;


        /**
         * Check if all handlers are ready
         *
         * @return true if all of our handlers are ready
         */
        bool allHandlersReady();

        /**
         * Allow a handler to set its state
         * @param moduleName the name of the module to set the state for
         * @param state the state to set
         * @return true if the state was set, false otherwise
         */
        Result<bool> setHandlerState(creatures::config::UARTDevice::module_name moduleName, MotorHandlerState state);

        /**
         * Get all of the handler IDs we're using
         *
         * @return a vector of module id
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

        // Messages in from the creature
        std::shared_ptr<MessageQueue<Message>> incomingQueue;

        // Messages out to the creature
        std::shared_ptr<MessageQueue<Message>> outgoingQueue;

        // A big 'ole map of module names to their incoming queues
        std::unordered_map<creatures::config::UARTDevice::module_name, HandlerQueues> servoHandlers;

        std::unordered_map<creatures::config::UARTDevice::module_name, MotorHandlerState> handlerStates;

    };
}