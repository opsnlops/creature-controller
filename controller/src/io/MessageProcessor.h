#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <string>
#include <unordered_map>

#include "controller-config.h"

#include "controller/ServoModuleHandler.h"
#include "io/handlers/IMessageHandler.h"
#include "io/handlers/InitHandler.h"
#include "io/handlers/LogHandler.h"
#include "io/handlers/PongHandler.h"
#include "io/handlers/ReadyHandler.h"
#include "io/handlers/BoardSensorHandler.h"
#include "io/handlers/MotorSensorHandler.h"
#include "io/handlers/StatsHandler.h"
#include "io/Message.h"
#include "io/MessageRouter.h"
#include "logging/Logger.h"
#include "server/ServerMessage.h"
#include "util/MessageQueue.h"
#include "util/Result.h"
#include "util/StoppableThread.h"


namespace creatures {


    class MessageProcessor : public StoppableThread {

    public:
        MessageProcessor(std::shared_ptr<Logger> logger,
                         UARTDevice::module_name moduleId,
                         std::shared_ptr<ServoModuleHandler> ServoModuleHandler,
                         std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue);
        ~MessageProcessor() = default;

        void registerHandler(std::string messageType, std::shared_ptr<IMessageHandler> handler);
        void start() override;
        void shutdown() override; // Added override keyword here

        void run() override;

        std::optional<Message> waitForMessage();
        Result<bool> processMessage(const Message& message);

    private:

        /**
         * Create the handlers before we try to use them :)
         */
        void createHandlers();

        std::shared_ptr<ServoModuleHandler> servoModuleHandler;
        std::shared_ptr<MessageQueue<Message>> incomingQueue;
        std::unordered_map<std::string, std::shared_ptr<IMessageHandler>> handlers;

        std::thread workerThread;

        // Flag to indicate if processor is shutting down
        std::atomic<bool> is_shutting_down;

        std::shared_ptr<Logger> logger;
        UARTDevice::module_name moduleId;


        std::shared_ptr<MessageQueue<creatures::server::ServerMessage>> websocketOutgoingQueue;

        // Handlers
        std::shared_ptr<creatures::LogHandler> logHandler;
        std::shared_ptr<creatures::InitHandler> initHandler;
        std::shared_ptr<creatures::PongHandler> pongHandler;
        std::shared_ptr<creatures::ReadyHandler> readyHandler;
        std::shared_ptr<creatures::StatsHandler> statsHandler;
        std::shared_ptr<creatures::BoardSensorHandler> boardSensorHandler;
        std::shared_ptr<creatures::MotorSensorHandler> motorSensorHandler;
    };

} // creatures