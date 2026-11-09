#pragma once

#include <atomic>
#include <chrono>
#include <optional>
#include <string>

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

namespace creatures::io {
class MessageRouter;
}

namespace creatures {
class MessageProcessor;

class ServoModuleHandler : public StoppableThread, public std::enable_shared_from_this<ServoModuleHandler> {

  public:
    ServoModuleHandler(std::shared_ptr<Logger> logger, std::shared_ptr<Controller> controller,
                       UARTDevice::module_name moduleId, std::string deviceNode,
                       std::shared_ptr<creatures::io::MessageRouter> messageRouter,
                       std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue,
                       std::chrono::milliseconds pongTimeout = std::chrono::seconds(PONG_TIMEOUT_SECONDS));

    ~ServoModuleHandler() override;

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
     * Note that the firmware answered a ping
     */
    void pongReceived();

    /**
     * Check if the link to the module has failed
     *
     * After an emergency stop this never reports a failure. The firmware is
     * expected to go silent, and exiting would risk clearing the stop on restart.
     *
     * @return a description of the failure, or nothing if the link is healthy
     */
    [[nodiscard]] std::optional<std::string> getCommunicationFailure() const;

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

    /**
     * The firmware version the module reported, or zero before it has
     */
    [[nodiscard]] u32 getFirmwareVersion() const;

  protected:
    void run() override; // This is the method that will be called when the
                         // thread starts

  private:
    // Flag to indicate if this module is shutting down
    std::atomic<bool> is_shutting_down{false};

    // The version of firmware our module is running
    // Zero until the firmware reports in
    std::atomic<u32> firmwareVersion{0};

    std::atomic<bool> ready{false};
    std::atomic<bool> configured{false};

    // Set if the serial port could not be opened at startup
    std::atomic<bool> serialStartFailed{false};

    // How long a ready module may go without answering a ping
    std::chrono::milliseconds pongTimeout;

    // When we last heard a pong, as steady_clock nanoseconds since its epoch
    std::atomic<std::chrono::steady_clock::rep> lastPongTime{0};

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