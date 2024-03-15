
#pragma once

#include <memory>
#include <thread>
#include <string>
#include <unordered_map>

#include "controller-config.h"

#include "controller/Controller.h"
#include "io/handlers/IMessageHandler.h"
#include "io/handlers/InitHandler.h"
#include "io/handlers/LogHandler.h"
#include "io/handlers/PongHandler.h"
#include "io/handlers/ReadyHandler.h"
#include "io/handlers/StatsHandler.h"
#include "io/Message.h"
#include "io/MessageRouter.h"
#include "util/MessageQueue.h"
#include "logging/Logger.h"


namespace creatures {

    using creatures::io::Message;
    using creatures::io::MessageRouter;

    class MessageProcessor {

    public:
        MessageProcessor(std::shared_ptr<Logger> logger,
                         std::shared_ptr<MessageRouter> messageRouter,
                         std::shared_ptr<Controller> controller);
        ~MessageProcessor() = default;

        void registerHandler(std::string messageType, std::shared_ptr<IMessageHandler> handler);
        void start();

        Message waitForMessage();
        void processMessage(const Message& message);

    private:

        /**
         * Create the handlers before we try to use them :)
         */
        void createHandlers();

        std::shared_ptr<MessageRouter> messageRouter;
        std::shared_ptr<MessageQueue<Message>> incomingQueue;
        std::unordered_map<std::string, std::shared_ptr<IMessageHandler>> handlers;

        std::thread workerThread;

        [[noreturn]] void processMessages();

        std::shared_ptr<Controller> controller;
        std::shared_ptr<Logger> logger;

        // Handlers
        std::shared_ptr<creatures::LogHandler> logHandler;
        std::shared_ptr<creatures::InitHandler> initHandler;
        std::shared_ptr<creatures::PongHandler> pongHandler;
        std::shared_ptr<creatures::ReadyHandler> readyHandler;
        std::shared_ptr<creatures::StatsHandler> statsHandler;
    };

} // creatures

