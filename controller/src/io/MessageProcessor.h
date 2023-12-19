
#pragma once

#include <memory>
#include <thread>
#include <string>
#include <unordered_map>

#include "controller-config.h"


#include "io/handlers/IMessageHandler.h"
#include "io/SerialHandler.h"
#include "util/MessageQueue.h"
#include "logging/Logger.h"


namespace creatures {

    class MessageProcessor {

    public:
        MessageProcessor(std::shared_ptr<Logger> logger, std::shared_ptr<SerialHandler> serialHandler);
        ~MessageProcessor() = default;

        void registerHandler(std::string messageType, std::unique_ptr<IMessageHandler> handler);
        void start();

        std::string waitForMessage();
        void processMessage(const std::string& message);

    private:
        std::shared_ptr<SerialHandler> serialHandler;
        std::shared_ptr<MessageQueue<std::string>> incomingQueue;
        std::unordered_map<std::string, std::unique_ptr<IMessageHandler>> handlers;

        std::thread workerThread;

        [[noreturn]] void processMessages();

        std::shared_ptr<Logger> logger;
    };

} // creatures

