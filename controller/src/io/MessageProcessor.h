
#pragma once

#include <memory>
#include <thread>
#include <unordered_map>

#include "controller-config.h"
#include "namespace-stuffs.h"

#include "io/handlers/IMessageHandler.h"
#include "io/SerialHandler.h"
#include "util/MessageQueue.h"


namespace creatures {

    class MessageProcessor {

    public:
        MessageProcessor(std::shared_ptr<SerialHandler> serialHandler);
        ~MessageProcessor() = default;

        void start();

    private:
        std::shared_ptr<SerialHandler> serialHandler;
        std::shared_ptr<MessageQueue<std::string>> incomingQueue;
        std::unordered_map<std::string, std::unique_ptr<IMessageHandler>> handlers;

        std::thread workerThread;

        [[noreturn]] void processMessages();
    };

} // creatures

