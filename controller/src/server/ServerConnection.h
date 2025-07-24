
#pragma once

#include <string>

#include <ixwebsocket/IXWebSocket.h>

#include "creature/Creature.h"
#include "logging/Logger.h"
#include "server/ServerMessage.h"
#include "server/WebsocketWriter.h"
#include "util/MessageQueue.h"
#include "util/Result.h"
#include "util/StoppableThread.h"

#include "controller-config.h"

namespace creatures ::server {

/**
 * A websocket connection to our server. This is optional, it doesn't need to be
 * used. The controller can run without it if needed, using only the e1.31
 * protocol.
 *
 * It's used for things like logging back to the server, so that the Creature
 * Console can see what's going on.
 */
class ServerConnection : public StoppableThread {

  public:
    ServerConnection(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<creatures::creature::Creature> creature, bool enabled,
        std::string address, u16 port,
        std::shared_ptr<
            creatures::MessageQueue<creatures::server::ServerMessage>>
            outgoingMessagesQueue)
        : logger(logger), enabled(enabled), address(std::move(address)),
          port(port), outgoingMessagesQueue(outgoingMessagesQueue),
          creature(creature) {

        webSocket = std::make_shared<ix::WebSocket>();

        websocketWriter = std::make_unique<WebsocketWriter>(
            logger, webSocket, outgoingMessagesQueue, creature->getId(),
            enabled);
    }
    ~ServerConnection();

    void start() override;

  protected:
    void run() override;

  private:
    std::shared_ptr<Logger> logger;

    bool enabled = false;
    std::string address;
    u16 port;

    std::string serverUrl;

    std::shared_ptr<creatures::MessageQueue<creatures::server::ServerMessage>>
        outgoingMessagesQueue;

    std::unique_ptr<WebsocketWriter> websocketWriter;

    // Our websocket connection
    std::shared_ptr<ix::WebSocket> webSocket;

    // The creature we're working on
    std::shared_ptr<creatures::creature::Creature> creature;

    std::string makeUrl() {
        return fmt::format("ws://{}:{}/api/v1/websocket", address, port);
    }

    /**
     * Callback called when the websocket receives a message
     *
     * @param msg the message to parse
     */
    void onMessage(const ix::WebSocketMessagePtr &msg);
};

} // namespace creatures::server
