#include <string>
#include <unordered_map>
#include <sstream>
#include <utility>
#include <vector>

#include "controller-config.h"

#include "config/UARTDevice.h"
#include "controller/ServoModuleHandler.h"
#include "io/handlers/InitHandler.h"
#include "io/handlers/LogHandler.h"
#include "io/handlers/PongHandler.h"
#include "io/handlers/StatsHandler.h"
#include "io/Message.h"
#include "io/MessageRouter.h"
#include "logging/Logger.h"
#include "util/Result.h"
#include "util/thread_name.h"

#include "MessageProcessor.h"
#include "io/MessageProcessingException.h"

namespace creatures {

    MessageProcessor::MessageProcessor(std::shared_ptr<Logger> _logger,
                                       UARTDevice::module_name moduleId,
                                       std::shared_ptr<ServoModuleHandler> servoModuleHandler,
                                       std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue) :
            servoModuleHandler(servoModuleHandler),
            logger(_logger),
            moduleId(moduleId),
            websocketOutgoingQueue(websocketOutgoingQueue),
            is_shutting_down(false) {

        logger->info("Message Processor created!");

        // Safety check - make sure we have a valid servoModuleHandler
        if (!servoModuleHandler) {
            logger->critical("ServoModuleHandler is null in MessageProcessor constructor");
            throw MessageProcessingException("ServoModuleHandler is null");
        }

        this->incomingQueue = this->servoModuleHandler->getIncomingQueue();

        // Safety check - make sure we have a valid incomingQueue
        if (!this->incomingQueue) {
            logger->critical("IncomingQueue is null in MessageProcessor constructor");
            throw MessageProcessingException("IncomingQueue is null");
        }

        createHandlers();

        // Register handlers
        registerHandler("LOG", this->logHandler);
        registerHandler("STATS", this->statsHandler);
        registerHandler("PONG", this->pongHandler);
        registerHandler("INIT", this->initHandler);
        registerHandler("READY", this->readyHandler);
        registerHandler("BSENSE", this->boardSensorHandler);
        registerHandler("MSENSE", this->motorSensorHandler);
    }

    void MessageProcessor::createHandlers() {
        logger->debug("creating the message handlers");

        this->logHandler = std::make_shared<LogHandler>();
        this->initHandler = std::make_shared<InitHandler>(this->logger, this->servoModuleHandler);
        this->pongHandler = std::make_shared<PongHandler>(this->logger, this->servoModuleHandler);
        this->statsHandler = std::make_shared<StatsHandler>();
        this->readyHandler = std::make_shared<ReadyHandler>(this->logger, this->servoModuleHandler);
        this->boardSensorHandler = std::make_shared<BoardSensorHandler>(this->logger, this->websocketOutgoingQueue);
        this->motorSensorHandler = std::make_shared<MotorSensorHandler>(this->logger, this->websocketOutgoingQueue);
    }

    void MessageProcessor::registerHandler(std::string messageType, std::shared_ptr<IMessageHandler> handler) {
        logger->info("registering handler for {}", messageType);
        handlers[messageType] = std::move(handler);
    }


    void MessageProcessor::start() {
        logger->info("Starting the message processor");
        creatures::StoppableThread::start();
    }

    void MessageProcessor::shutdown() {
        logger->info("Shutting down the message processor");
        is_shutting_down.store(true);
        creatures::StoppableThread::shutdown();
    }

    /**
     * Wait for a message to be delivered from the incoming queue with a timeout
     *
     * @return the next message, or empty message on timeout
     */
    std::optional<Message> MessageProcessor::waitForMessage() {
        logger->trace("waiting for a message");

        // Make sure we have a valid queue
        if (!this->incomingQueue) {
            logger->error("IncomingQueue is null in waitForMessage");
            throw MessageProcessingException("IncomingQueue is null");
        }

        try {
            auto incomingMessage = this->incomingQueue->popWithTimeout(std::chrono::milliseconds(100));
            logger->trace("got one!");
            return incomingMessage;
        } catch (const std::exception& e) {
            // Timeout - this is normal, just return an empty optional
            return std::nullopt;
        }
    }

    /**
     * Process a message
     *
     * @param message the message to process
     */
    Result<bool> MessageProcessor::processMessage(const Message& message) {
        // Don't process messages if we're shutting down
        if (is_shutting_down.load()) {
            return Result<bool>{ControllerError(ControllerError::UnprocessableMessage, "MessageProcessor is shutting down")};
        }

#if DEBUG_MESSAGE_PROCESSING
        this->logger->debug("processing message: {}", message.payload);
#endif

        // Make sure there's actually handlers registered
        if(handlers.empty()) {
            auto errorMessage = "There's no handlers registered and you're asking me to process a message!";
            logger->critical(errorMessage);
            return Result<bool>{ControllerError(ControllerError::UnprocessableMessage, errorMessage)};
        }

        // Make sure the message has content
        if (message.payload.empty()) {
            return Result<bool>{true}; // Empty message, nothing to do
        }

        // Tokenize message
        std::istringstream iss(message.payload);
        std::vector<std::string> tokens;
        std::string token;
        while (std::getline(iss, token, '\t')) {
            tokens.push_back(token);
        }

        // Make sure we have at least one token
        if (tokens.empty()) {
            auto errorMessage = "Message has no tokens";
            logger->warn(errorMessage);
            return Result<bool>{ControllerError(ControllerError::UnprocessableMessage, errorMessage)};
        }

        // Find and invoke the handler
        auto it = handlers.find(tokens[0]);
        if (it != handlers.end()) {
            // Handler found, make sure it's valid
            if (!it->second) {
                auto errorMessage = fmt::format("Handler for {} is null", tokens[0]);
                logger->error(errorMessage);
                return Result<bool>{ControllerError(ControllerError::UnprocessableMessage, errorMessage)};
            }

            try {
                // Invoke the handler
                it->second->handle(logger, tokens);
            } catch (const std::exception& e) {
                auto errorMessage = fmt::format("Exception in message handler for {}: {}", tokens[0], e.what());
                logger->error(errorMessage);
                return Result<bool>{ControllerError(ControllerError::UnprocessableMessage, errorMessage)};
            }
        } else {
            // We didn't find a handler!

            // Sanitize the input to ensure it's null-terminated in the case of junk data
            // coming in off the wire.
            std::string safeToken = tokens[0];
            std::string errorMessage = fmt::format("Unknown message type: {}", safeToken);
            logger->error(errorMessage);
            return Result<bool>{ControllerError(ControllerError::UnprocessableMessage, errorMessage)};
        }

        return Result<bool>{true};
    }

    void MessageProcessor::run() {
        auto threadName = fmt::format("MessageProcessor::{}", UARTDevice::moduleNameToString(this->moduleId));
        setThreadName(threadName);

        logger->debug("hello from the message processor thread for {}! 👋🏻", UARTDevice::moduleNameToString(servoModuleHandler->getModuleName()));

        while(!this->stop_requested.load()) {
            // Check if we're shutting down
            if (is_shutting_down.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            try {
                auto messageOpt = waitForMessage();

                // Skip processing if no message (timeout)
                if (!messageOpt) {
                    continue;
                }

                // Try to process the message, leaving an error if needed
                auto processResult = processMessage(messageOpt.value());
                if(!processResult.isSuccess()) {
                    auto errorMessage = fmt::format("Error processing message: {}", processResult.getError()->getMessage());
                    logger->error(errorMessage);
                    // Don't stop the thread, just log the error and continue
                }
            } catch (const std::exception& e) {
                // Catch any exceptions to prevent thread termination
                logger->error("Exception in message processor loop: {}", e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        logger->info("MessageProcessor thread for {} stopping", UARTDevice::moduleNameToString(this->moduleId));
    }

} // creatures