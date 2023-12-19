
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

namespace creatures {

    MessageProcessor::MessageProcessor(std::shared_ptr<Logger> logger, std::shared_ptr<SerialHandler> serialHandler) :
        logger(std::move(logger)) {


        logger->info("Message Processor created!");

        this->serialHandler = std::move(serialHandler);
        this->incomingQueue = this->serialHandler->getIncomingQueue();

        // Register the handlers
        handlers["LOG"] = std::make_unique<LogHandler>();
        handlers["STATS"] = std::make_unique<StatsHandler>();

    }

    void MessageProcessor::start() {

        logger->info("Starting the message processor");

        this->workerThread = std::thread(&MessageProcessor::processMessages, this);
        this->workerThread.detach(); // Bye bye little thread!

    }

    [[noreturn]] void MessageProcessor::processMessages() {

        setThreadName("MessageProcessor::processMessages");

        logger->debug("hello from the message processor thread! 👋🏻");

        for(EVER) {

            auto incomingMessage = this->incomingQueue->pop();

#if DEBUG_MESSAGE_PROCESSING
            this->logger->debug("pulled message off queue: {}", incomingMessage);
#endif

            // Tokenize message
            std::istringstream iss(incomingMessage);
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
                // Handler not found, handle this situation (log, ignore, etc.)
                logger->error("Unknown message type: {}", tokens[0]);
            }

        }


    }

} // creatures