
#include <string>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <utility>
#include <vector>

#include "controller-config.h"

#include "io/handlers/LogHandler.h"
#include "io/handlers/StatsHandler.h"
#include "util/thread_name.h"

#include "MessageProcessor.h"
#include "io/MessageProcessingException.h"

namespace creatures {

    MessageProcessor::MessageProcessor(std::shared_ptr<Logger> logger, std::shared_ptr<SerialHandler> serialHandler) :
        logger(logger) {


        logger->info("Message Processor created!");

        this->serialHandler = std::move(serialHandler);
        this->incomingQueue = this->serialHandler->getIncomingQueue();

        // Register handlers
        registerHandler("LOG", std::make_unique<LogHandler>());
        registerHandler("STATS", std::make_unique<StatsHandler>());
    }

    void MessageProcessor::registerHandler(std::string messageType, std::unique_ptr<IMessageHandler> handler) {
        logger->info("registering handler for {}", messageType);
        handlers[messageType] = std::move(handler);
    }


    void MessageProcessor::start() {

        logger->info("Starting the message processor");

        this->workerThread = std::thread(&MessageProcessor::processMessages, this);
        this->workerThread.detach(); // Bye bye little thread!

    }

    /**
     * Wait for a message to be delivered from the incoming queue
     *
     * @return the next message
     */
    std::string MessageProcessor::waitForMessage() {
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
    void MessageProcessor::processMessage(const std::string& message) {

#if DEBUG_MESSAGE_PROCESSING
        this->logger->debug("pulled message off queue: {}", incomingMessage);
#endif

        // Make sure there's actually handlers registered
        if(handlers.empty()) {
            logger->critical("There's no handlers registered and you're asking me to process a message!");
            throw MessageProcessingException("No handlers registered!");
        }

        // Tokenize message
        std::istringstream iss(message);
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
            std::string errorMessage = fmt::format("Unknown message type: {}", tokens[0]);
            logger->error(errorMessage);
            throw MessageProcessingException(errorMessage);
        }

    }

    [[noreturn]] void MessageProcessor::processMessages() {

        setThreadName("MessageProcessor::processMessages");

        logger->debug("hello from the message processor thread! 👋🏻");

        for(EVER) {
            auto message = waitForMessage();

            // Try to process the message, leaving an error if needed
            try {
                processMessage(message);
            } catch (const MessageProcessingException& e) {
                logger->error(e.what());
            }
        }


    }

} // creatures