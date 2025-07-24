
#include <chrono>

#include <ixwebsocket/IXUserAgent.h>
#include <ixwebsocket/IXWebSocket.h>

#include "logging/Logger.h"
#include "util/thread_name.h"

#include "server/ServerConnection.h"

#define SERVER_CONNECTION_LOOP_TIME                                            \
    500 // How long should we wait between checking to see if we should stop?

namespace creatures::server {

ServerConnection::~ServerConnection() {
    logger->info("server connection destroyed");
}

void ServerConnection::start() {

    // We always need to make the writer, even if we're not enabled so that
    // messages that get sent don't just sit in the queue forever and leak
    // memory.
    websocketWriter = std::make_unique<WebsocketWriter>(
        logger, webSocket, outgoingMessagesQueue, creature->getId(), enabled);
    websocketWriter->start();

    if (!enabled) {
        logger->info("server connection is disabled, not starting");
        return;
    }

    serverUrl = makeUrl();
    logger->info("server url: {}", serverUrl);
    webSocket->setUrl(serverUrl);

    webSocket->setOnMessageCallback(
        [this](auto &&PH1) { onMessage(std::forward<decltype(PH1)>(PH1)); });

    logger->info("starting the server connection");
    creatures::StoppableThread::start();
}

void ServerConnection::run() {

    // Don't do anything if we're not enabled
    if (!enabled) {
        return;
    }

    // Set thread name
    setThreadName("creatures::server::ServerConnection");
    logger->info("hello from the Creature Server connection!");

    // How often should we see if we need to stop?
    std::chrono::milliseconds interval(SERVER_CONNECTION_LOOP_TIME);

    webSocket->start();
    webSocket->setPingInterval(10);

    // Wait until we're told to stop
    while (!stop_requested.load()) {
        std::this_thread::sleep_for(interval);
    }
    logger->debug("ServerConnection thread stopping");

    // Stop our threads (we need to do this ourselves since it's a unique ptr)
    websocketWriter->shutdown();

    webSocket->stop();
    logger->info("Creature Server connection shut down");
}

void ServerConnection::onMessage(const ix::WebSocketMessagePtr &msg) {

    logger->trace("websocket received message");

    if (msg->type == ix::WebSocketMessageType::Message) {
        logger->debug("received message: {}", msg->str);
    } else if (msg->type == ix::WebSocketMessageType::Open) {
        logger->info("Connection established");
    } else if (msg->type == ix::WebSocketMessageType::Error) {
        logger->error("Connection error: {}", msg->errorInfo.reason);
    }
}

} // namespace creatures::server
