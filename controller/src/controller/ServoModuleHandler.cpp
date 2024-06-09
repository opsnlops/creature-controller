
#include <utility>

#include "config/UARTDevice.h"
#include "controller/ServoModuleHandler.h"
#include "controller/commands/ServoModuleConfiguration.h"
#include "io/Message.h"
#include "io/MessageProcessor.h"
#include "io/MessageRouter.h"
#include "io/SerialHandler.h"
#include "logging/Logger.h"
#include "util/MessageQueue.h"
#include "util/StoppableThread.h"
#include "util/thread_name.h"

namespace creatures {

    using creatures::MessageProcessor;
    using creatures::SerialHandler;
    using creatures::config::UARTDevice;
    using creatures::io::Message;
    using creatures::io::MessageRouter;

    ServoModuleHandler::ServoModuleHandler(std::shared_ptr<Logger> logger,
                                           std::shared_ptr<Controller> controller,
                                           UARTDevice::module_name moduleId, std::string deviceNode,
                                           std::shared_ptr<MessageRouter> messageRouter) :
                                           logger(logger), controller(controller), deviceNode(std::move(deviceNode)),
                                           moduleId(moduleId), messageRouter(messageRouter) {

        logger->info("creating a new ServoModuleHandler for module {} on node {}",
                     UARTDevice::moduleNameToString(moduleId), this->deviceNode);

        // Make our queues
        this->outgoingQueue = std::make_shared<MessageQueue<Message>>();
        this->incomingQueue = std::make_shared<MessageQueue<Message>>();

        this->messageRouter->setHandlerState(this->moduleId, creatures::io::MotorHandlerState::idle);

        this->threadName = fmt::format("ServoModuleHandler-{}", UARTDevice::moduleNameToString(this->moduleId));

    }

    void ServoModuleHandler::init() {
        logger->info("initializing the ServoModuleHandler for module {} on node {}",
                     UARTDevice::moduleNameToString(this->moduleId), this->deviceNode);

        // Make our message processor
        this->messageProcessor = std::make_unique<MessageProcessor>(logger, moduleId, shared_from_this());

        // Create the SerialHandler
        this->serialHandler = std::make_shared<SerialHandler>(logger, this->deviceNode, this->moduleId,
                                                             this->outgoingQueue, this->incomingQueue);

        this->messageRouter->setHandlerState(this->moduleId, creatures::io::MotorHandlerState::awaitingConfiguration);

    }

    void ServoModuleHandler::shutdown() {
        logger->info("shutting down the ServoModuleHandler for module {} on node {}",
                     UARTDevice::moduleNameToString(this->moduleId), this->deviceNode);

        // Gracefully shut down the SerialHandler
        logger->debug("shutting down the SerialHandler for module {} on node {}",
                      UARTDevice::moduleNameToString(this->moduleId), this->deviceNode);
        for(const auto& t : this->serialHandler->threads) {
            logger->debug("stopping thread {}", t->getName());
            t->shutdown();
        }

        // Tell the message router we've stopped
        this->messageRouter->setHandlerState(this->moduleId, creatures::io::MotorHandlerState::stopped);

        this->serialHandler->shutdown();
        this->messageProcessor->shutdown();
    }

    void ServoModuleHandler::start() {
        this->messageProcessor->start();
        this->serialHandler->start();
        creatures::StoppableThread::start();
    }

    std::shared_ptr<MessageQueue<Message>> ServoModuleHandler::getIncomingQueue() {
        return this->incomingQueue;
    }

    std::shared_ptr<MessageQueue<Message>> ServoModuleHandler::getOutgoingQueue() {
        return this->outgoingQueue;
    }


    Result<bool> ServoModuleHandler::firmwareReadyForInitialization(u32 firmwareVersion) {

        // Make sure we got the version of the firmware we were built against
        if(firmwareVersion != FIRMWARE_VERSION) {
            std::string errorMessage = fmt::format("Firmware version mismatch on module {}! Expected {}, got {}",
                                                   UARTDevice::moduleNameToString(getModuleName()),
                                                   FIRMWARE_VERSION,
                                                   firmwareVersion);
            logger->critical(errorMessage);
            return Result<bool>{ControllerError(ControllerError::InvalidConfiguration, errorMessage)};
        }

        logger->debug("firmware is ready for initialization (version {})", firmwareVersion);
        this->messageRouter->setHandlerState(this->moduleId, creatures::io::MotorHandlerState::configuring);

        // Go gather the configuration from the creature
        auto creatureConfigCommand = creatures::commands::ServoModuleConfiguration(logger);
        creatureConfigCommand.getServoConfigurations(controller, getModuleName());

        // ...and toss it to the serial handler
        creatures::io::Message message = creatures::io::Message(getModuleName(), creatureConfigCommand.toMessageWithChecksum());

        auto sendResult = this->messageRouter->sendMessageToCreature(message);
        if(!sendResult.isSuccess()) {
            logger->critical("Failed to send the creature configuration to the firmware: {}", sendResult.getError()->getMessage());
        }

        return Result<bool>{true};

    }


    void ServoModuleHandler::firmwareReadyToOperate() {
        logger->info("firmware is ready to operate");
        this->ready = true;
        this->messageRouter->setHandlerState(this->moduleId, creatures::io::MotorHandlerState::ready);
    }

    Result<bool> ServoModuleHandler::sendMessageToController(std::string messagePayload) {
        logger->trace("sending message to controller: {}", messagePayload);

        auto message = Message(this->moduleId, messagePayload);

        auto result = this->messageRouter->receivedMessageFromCreature(message);
        if(!result.isSuccess()) {
            auto errorMessage = fmt::format("Failed to send message to the message router: {}", result.getError()->getMessage());
            logger->error(errorMessage);
            return result;
        }
        return Result<bool>{true};
    }


    void ServoModuleHandler::run() {
        setThreadName(threadName);

        logger->info("ServoModuleHandler thread started");

        while(!stop_requested.load()) {

            // Wait for a message to come in from one of our modules
            auto incomingMessage = this->incomingQueue->pop();
            this->logger->trace("incoming message: {}", incomingMessage.payload);

            // Go process it!
            messageProcessor->processMessage(incomingMessage);
        }

        logger->info("ServoModuleHandler thread stopping");

    }

    creatures::config::UARTDevice::module_name ServoModuleHandler::getModuleName() const {
        return this->moduleId;
    }

} // creatures