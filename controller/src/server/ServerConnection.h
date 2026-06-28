
#pragma once

#include <atomic>
#include <functional>
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
    ServerConnection(std::shared_ptr<Logger> _logger, std::shared_ptr<creatures::creature::Creature> _creature,
                     bool _enabled, std::string _address, u16 _port,
                     std::shared_ptr<creatures::MessageQueue<creatures::server::ServerMessage>> _outgoingMessagesQueue)
        : logger(std::move(_logger)), enabled(_enabled), address(std::move(_address)), port(_port),
          outgoingMessagesQueue(std::move(_outgoingMessagesQueue)), creature(std::move(_creature)) {

        webSocket = std::make_shared<ix::WebSocket>();

        websocketWriter =
            std::make_unique<WebsocketWriter>(logger, webSocket, outgoingMessagesQueue, creature->getId(), enabled);
    }
    ~ServerConnection();

    void start() override;

    /**
     * Register a callback to run whenever the websocket connection to the server
     * is (re)established - on the initial connect AND after every reconnect
     * (e.g. the server restarting).
     *
     * This is how the controller re-registers itself with the server
     * automatically: the server forgets its registrations across a restart, so
     * a controller that only registered once at startup would silently go dark
     * until manually restarted. Tying (re-)registration to the websocket
     * reconnect means a server restart is recovered from on its own.
     *
     * Must be set before start(). The callback runs on this object's own thread,
     * not the websocket's, so it may block (e.g. on an HTTP POST).
     */
    void setConnectionEstablishedCallback(std::function<void()> callback) {
        connectionEstablishedCallback = std::move(callback);
    }

  protected:
    void run() override;

  private:
    std::shared_ptr<Logger> logger;

    bool enabled = false;
    std::string address;
    u16 port;

    std::string serverUrl;

    std::shared_ptr<creatures::MessageQueue<creatures::server::ServerMessage>> outgoingMessagesQueue;

    std::unique_ptr<WebsocketWriter> websocketWriter;

    // Our websocket connection
    std::shared_ptr<ix::WebSocket> webSocket;

    // The creature we're working on
    std::shared_ptr<creatures::creature::Creature> creature;

    // Set by the websocket Open event, consumed by run(). This hands the work
    // off the websocket's callback thread so the (possibly blocking)
    // connection-established callback runs on our own thread instead.
    std::atomic<bool> connectionEstablished{false};

    // Run on every websocket (re)connect; see setConnectionEstablishedCallback().
    std::function<void()> connectionEstablishedCallback;

    std::string makeUrl() { return fmt::format("ws://{}:{}/api/v1/websocket", address, port); }

    /**
     * Callback called when the websocket receives a message
     *
     * @param msg the message to parse
     */
    void onMessage(const ix::WebSocketMessagePtr &msg);
};

} // namespace creatures::server
