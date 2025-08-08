#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "controller-config.h"

#include "config/UARTDevice.h"
#include "controller/ServoModuleHandler.h"
#include "io/Message.h"
#include "io/handlers/InitHandler.h"
#include "io/handlers/LogHandler.h"
#include "io/handlers/PongHandler.h"
#include "io/handlers/StatsHandler.h"
#include "logging/Logger.h"
#include "util/Result.h"
#include "util/thread_name.h"

#include "MessageProcessor.h"
#include "io/MessageProcessingException.h"

namespace creatures {

MessageProcessor::MessageProcessor(
    std::shared_ptr<Logger> _logger, UARTDevice::module_name moduleId,
    std::shared_ptr<ServoModuleHandler> servoModuleHandler,
    std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue)
    : servoModuleHandler(servoModuleHandler), logger(_logger), moduleId(moduleId),
      websocketOutgoingQueue(websocketOutgoingQueue) {

    logger->info("MessageProcessor created for module {}", UARTDevice::moduleNameToString(moduleId));

    // Basic validation - fail fast if something is wrong
    if (!servoModuleHandler) {
        logger->critical("ServoModuleHandler is null in MessageProcessor constructor");
        throw MessageProcessingException("ServoModuleHandler is null");
    }

    this->incomingQueue = this->servoModuleHandler->getIncomingQueue();
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

/**
 * Process a message
 *
 * @param message the message to process
 */
Result<bool> MessageProcessor::processMessage(const Message &message) {

#if DEBUG_MESSAGE_PROCESSING
    this->logger->debug("processing message: {}", message.payload);
#endif

    // Make sure we have handlers registered
    if (handlers.empty()) {
        auto errorMessage = "No handlers registered!";
        logger->critical(errorMessage);
        return Result<bool>{ControllerError(ControllerError::UnprocessableMessage, errorMessage)};
    }

    // Skip empty messages
    if (message.payload.empty()) {
        return Result<bool>{true};
    }

    // Tokenize message by tabs
    std::istringstream iss(message.payload);
    std::vector<std::string> tokens;
    std::string token;
    while (std::getline(iss, token, '\t')) {
        tokens.push_back(token);
    }

    // Need at least one token (the command)
    if (tokens.empty()) {
        logger->warn("Message has no tokens: '{}'", message.payload);
        return Result<bool>{true}; // Not really an error, just skip it
    }

    // Find and invoke the handler
    auto it = handlers.find(tokens[0]);
    if (it != handlers.end()) {
        try {
            // Handler found, invoke it
            it->second->handle(logger, tokens);
        } catch (const std::exception &e) {
            auto errorMessage = fmt::format("Exception in message handler for {}: {}", tokens[0], e.what());
            logger->error(errorMessage);
            // Don't fail - just log the error and continue
        }
    } else {
        // Unknown message type - log but don't fail
        logger->warn("Unknown message type: '{}'", tokens[0]);
    }

    return Result<bool>{true};
}

void MessageProcessor::run() {
    this->threadName = fmt::format("MessageProcessor::{}", UARTDevice::moduleNameToString(this->moduleId));
    setThreadName(this->threadName);

    logger->debug("MessageProcessor thread started for module {}", UARTDevice::moduleNameToString(this->moduleId));

    while (!this->stop_requested.load()) {
        // Wait for a message with a short timeout so we can check stop_requested
        auto messageOpt = this->incomingQueue->pop_timeout(std::chrono::milliseconds(100));

        if (!messageOpt.has_value()) {
            // Timeout - just continue to check stop_requested
            continue;
        }

        // Process the message
        auto processResult = processMessage(messageOpt.value());
        if (!processResult.isSuccess()) {
            // Log the error but continue processing
            auto errorMessage = fmt::format("Error processing message: {}", processResult.getError()->getMessage());
            logger->error(errorMessage);
        }
    }

    logger->info("MessageProcessor thread for {} stopping gracefully", UARTDevice::moduleNameToString(this->moduleId));
}

} // namespace creatures