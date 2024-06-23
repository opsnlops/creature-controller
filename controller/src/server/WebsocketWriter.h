
#pragma once

#include <string>
#include <thread>

#include <ixwebsocket/IXWebSocket.h>

#include "logging/Logger.h"
#include "server/ServerMessage.h"
#include "util/MessageQueue.h"
#include "util/StoppableThread.h"

#include "controller-config.h"

namespace creatures::server {


    /**
     * @brief A thread that writes messages to the websocket
     *
     * Inspired by the SerialWriter class in the io module, this class
     * writes messages out to the WebSocket. It's a lot easier since
     * we don't have hardware we're juggling.
     */
    class WebsocketWriter : public StoppableThread {

    public:

        WebsocketWriter(const std::shared_ptr<Logger>& logger,
                        std::shared_ptr<ix::WebSocket> webSocket,
                        const std::shared_ptr<MessageQueue<ServerMessage>>& outgoingQueue,
                        std::string creatureId,
                        bool enabled);

        ~WebsocketWriter() override {
            this->logger->info("WebsocketWriter destroyed");
        }

        void start() override;

    protected:
        void run() override;

    private:
        bool enabled;
        std::string creatureId;
        std::shared_ptr<ix::WebSocket> webSocket;
        std::shared_ptr<Logger> logger;
        std::shared_ptr<MessageQueue<ServerMessage>> outgoingQueue;
    };

} // creatures::server