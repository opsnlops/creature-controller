#pragma once

#include <atomic>

#include "config/UARTDevice.h"
#include "controller/Controller.h"
#include "io/Message.h"
#include "io/MessageRouter.h"
#include "io/SerialHandler.h"
#include "logging/Logger.h"
#include "server/ServerMessage.h"
#include "util/MessageQueue.h"
#include "util/StoppableThread.h"

class Controller;

namespace creatures ::io {
class MessageRouter;
}

namespace creatures {
class MessageProcessor;

class ServoModuleHandler : public StoppableThread, public std::enable_shared_from_this<ServoModuleHandler> {

  public:
    ServoModuleHandler(std::shared_ptr<Logger> logger, std::shared_ptr<Controller> controller,
                       UARTDevice::module_name moduleId, std::string deviceNode,
                       std::shared_ptr<creatures::io::MessageRouter> messageRouter,
                       std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue);

    void init();

    void start() override;
    void shutdown() override;

    void send(const Message &message);
    void resetServoModule();

    /**
     * Send a message to the remote device with it's configuration.
     *
     * It's up to the controller to properly format the configuration string.
     * We're just the messenger.
     *
     * @param controllerConfiguration the configuration string to send to the
     * remote device
     */
    void sendControllerConfiguration(std::string controllerConfiguration);

    /**
     * Return a pointer to our incoming queue for the processor to use
     *
     * @return a `MessageQueue<Message>` for incoming messages FROM the remote
     * device
     */
    std::shared_ptr<MessageQueue<Message>> getIncomingQueue();

    /**
     * Return a pointer to our incoming queue for the processor to use
     *
     * @return a `MessageQueue<Message>` for incoming messages TO the remote
     * device
     */
    std::shared_ptr<MessageQueue<Message>> getOutgoingQueue();

    /**
     * Send a message back to the controller
     *
     * @param messagePayload the string to send
     * @return a Result<bool> indicating success or failure
     */
    Result<bool> sendMessageToController(std::string messagePayload);

    /**
     * @brief Informs the controller that the firmware is ready for
     * initialization
     *
     * This is called by the InitHandler when we get a message from the firmware
     * that it's showtime!
     */
    Result<bool> firmwareReadyForInitialization(u32 firmwareVersion);

    /**
     * @brief Tells the controller that the firmware is ready to operate
     *
     * This is set by ReadyHandler.
     */
    void firmwareReadyToOperate();

    /**
     * Get the module name of the module we're controlling
     *
     * @return the module name of the module we're controlling
     */
    creatures::config::UARTDevice::module_name getModuleName() const;

    /**
     * Check if the module is ready to operate
     *
     * @return true if the module is ready
     */
    bool isReady() const;

    /**
     * Check if the module is configured
     *
     * @return true if the module is configured
     */
    bool isConfigured() const;

  protected:
    void run() override; // This is the method that will be called when the
                         // thread starts

  private:
    // Flag to indicate if this module is shutting down
    std::atomic<bool> is_shutting_down{false};

    // The version of firmware our module is running
    u32 firmwareVersion;

    std::atomic<bool> ready{false};
    std::atomic<bool> configured{false};

    /**
     * Our logger
     */
    std::shared_ptr<Logger> logger;

    /**
     * Our controller
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
     * The message router we're using to route messages from our device to the
     * controller
     */
    std::shared_ptr<creatures::io::MessageRouter> messageRouter;

    /*
     * Our message processor
     */
    std::unique_ptr<creatures::MessageProcessor> messageProcessor;

    /**
     * Our websocket outgoing queue
     */
    std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue;
};

} // namespace creatures