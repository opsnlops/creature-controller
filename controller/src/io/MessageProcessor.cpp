
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
                                       websocketOutgoingQueue(websocketOutgoingQueue) {

        logger->info("Message Processor created!");
        this->incomingQueue = this->servoModuleHandler->getIncomingQueue();

        createHandlers();

        // Register handlers
        registerHandler("LOG", this->logHandler);
        registerHandler("STATS", this->statsHandler);
        registerHandler("PONG", this->pongHandler);
        registerHandler("INIT", this->initHandler);
        registerHandler("READY", this->readyHandler);
        registerHandler("SENSOR", this->sensorHandler);
    }

    void MessageProcessor::createHandlers() {
        logger->debug("creating the message handlers");

        this->logHandler = std::make_shared<LogHandler>();
        this->initHandler = std::make_shared<InitHandler>(this->logger, this->servoModuleHandler);
        this->pongHandler = std::make_shared<PongHandler>(this->logger, this->servoModuleHandler);
        this->statsHandler = std::make_shared<StatsHandler>();
        this->readyHandler = std::make_shared<ReadyHandler>(this->logger, this->servoModuleHandler);
        this->sensorHandler = std::make_shared<SensorHandler>(this->logger, this->websocketOutgoingQueue);

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
     * Wait for a message to be delivered from the incoming queue
     *
     * @return the next message
     */
    Message MessageProcessor::waitForMessage() {
        logger->trace("waiting for a message");
        auto incomingMessage = this->incomingQueue->pop();
        logger->trace("got one!");
        return incomingMessage;
    }

    /**
     * Process a message
     *
     * @param message the message to process
     */
    Result<bool> MessageProcessor::processMessage(const Message& message) {

#if DEBUG_MESSAGE_PROCESSING
        this->logger->debug("pulled message off queue: {}", incomingMessage);
#endif

        // Make sure there's actually handlers registered
        if(handlers.empty()) {
            auto errorMessage = "There's no handlers registered and you're asking me to process a message!";
            logger->critical(errorMessage);
            return Result<bool>{ControllerError(ControllerError::UnprocessableMessage, errorMessage)};
        }

        // Tokenize message
        std::istringstream iss(message.payload);
        std::vector<std::string> tokens;
        std::string token;
        while (std::getline(iss, token, '\t')) {
            tokens.push_back(token);
        }

        // Find and invoke the handler
        auto it = handlers.find(tokens[0]);
        if (it != handlers.end()) {
            // Handler found, invoke it
            it->second->handle(logger, tokens);
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

            auto message = waitForMessage();

            // Try to process the message, leaving an error if needed
            auto processResult = processMessage(message);
            if(!processResult.isSuccess()) {
                auto errorMessage = fmt::format("Error processing message: {}", processResult.getError()->getMessage());
                logger->error(errorMessage);

                // Don't stop the thread, just log the error
            }
        }
    }

} // creatures