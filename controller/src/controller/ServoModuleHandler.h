
#pragma once

#include "config/UARTDevice.h"
#include "controller/Controller.h"
#include "io/Message.h"
#include "io/MessageRouter.h"
#include "io/SerialHandler.h"
#include "logging/Logger.h"
#include "util/MessageQueue.h"
#include "util/StoppableThread.h"

namespace creatures {
    class MessageProcessor;

    class ServoModuleHandler : public StoppableThread, public std::enable_shared_from_this<ServoModuleHandler> {

    public:
        ServoModuleHandler(std::shared_ptr<Logger> logger,
                           std::shared_ptr<Controller> controller,
                           UARTDevice::module_name moduleId,
                           std::string deviceNode,
                           std::shared_ptr<creatures::io::MessageRouter> messageRouter);

        void init();

        void start();
        void shutdown();

        bool isReady() const;
        bool isConfigured() const;

        void send(const Message& message);
        void resetServoModule();

        /**
         * Send a message to the remote device with it's configuration.
         *
         * It's up to the controller to properly format the configuration string. We're just the
         * messenger.
         *
         * @param controllerConfiguration the configuration string to send to the remote device
         */
        void sendControllerConfiguration(std::string controllerConfiguration);

        /**
         * Return a pointer to our incoming queue for the processor to use
         *
         * @return a `MessageQueue<Message>` for incoming messages FROM the remote device
         */
        std::shared_ptr<MessageQueue<Message>> getIncomingQueue();

        void firmwareReadyToOperate();

    protected:
        void run(); // This is the method that will be called when the thread starts

    private:

        // The version of firmware our module is running
        u32 firmwareVersion;

        bool ready = false;
        bool configured = false;





        /**
         * Our logger
         */
        std::shared_ptr<Logger> logger;

        /**
         * The controller we're working for
         */
        std::shared_ptr<Controller> controller;

        /**
         * The device node we're using to communicate with the module
         */
        std::string deviceNode;

        /**
         * The ID of the module we're controlling
         */
        UARTDevice::module_name moduleId;

        /**
         * The serial handler we're using to communicate with the module
         *
         * This is created when init() is called
         */
        std::shared_ptr<SerialHandler> serialHandler;

        /**
         * A `MessageQueue<Message>` for outgoing messages TO the remote device
         */
        std::shared_ptr<MessageQueue<Message>> outgoingQueue;

        /**
         * A `MessageQueue<Message>` for incoming messages FROM the remote device
         */
        std::shared_ptr<MessageQueue<Message>> incomingQueue;


        /**
         * The message router we're using to route messages from our device to the controller
         */
        std::shared_ptr<creatures::io::MessageRouter> messageRouter;

        /*
         * Our message processor
         */
        std::unique_ptr<creatures::MessageProcessor> messageProcessor;

    };

} // namespace creatures